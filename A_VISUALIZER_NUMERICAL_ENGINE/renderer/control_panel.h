#pragma once
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

namespace bkqrgr_ui {

enum class SliderScale { Linear, Logarithmic };

struct SliderSpec {
    std::wstring label;
    double minimum = 0.0;
    double maximum = 1.0;
    double step = 0.0;
    double defaultValue = 0.0;
    SliderScale scale = SliderScale::Linear;
    std::function<double()> get;
    std::function<void(double)> set;
    std::function<std::wstring(double)> format;
};

struct ToggleSpec {
    std::wstring label;
    bool defaultValue = false;
    std::function<bool()> get;
    std::function<void(bool)> set;
    std::wstring onText = L"ON";
    std::wstring offText = L"OFF";
};

class ControlPanel {
public:
    ControlPanel(HWND parent, int x, int y, int w, int h,
                 std::wstring title=L"BINARY CONTROL DECK",
                 std::wstring hint=L"F5 or button: full restart  •  wheel scrolls  •  double-click resets slider")
        : parent_(parent), width_(w), height_(h), titleText_(std::move(title)), hintText_(std::move(hint)) {
        register_class();
        // Own all GDI objects before the HWND becomes paintable.  This avoids
        // creation-time WM_PAINT observing a half-constructed ControlPanel.
        titleFont_ = make_font(19, FW_SEMIBOLD);
        labelFont_ = make_font(14, FW_MEDIUM);
        valueFont_ = make_font(13, FW_SEMIBOLD);
        hintFont_ = make_font(12, FW_NORMAL);
        hwnd_ = CreateWindowExW(0, class_name(), L"BKQR controls",
                                WS_CHILD | WS_CLIPSIBLINGS,
                                x, y, w, h, parent, nullptr, GetModuleHandleW(nullptr), this);
        if (!hwnd_) throw std::runtime_error("ControlPanel CreateWindowEx failed");
        ShowWindow(hwnd_,SW_SHOW);
        UpdateWindow(hwnd_);
    }

    ~ControlPanel() {
        if (hwnd_ && IsWindow(hwnd_)) {
            if(GetCapture()==hwnd_) ReleaseCapture();
            DestroyWindow(hwnd_);
        }
        restartButton_=nullptr;
        playButton_=nullptr;
        hwnd_=nullptr;
        if (titleFont_) DeleteObject(titleFont_);
        if (labelFont_) DeleteObject(labelFont_);
        if (valueFont_) DeleteObject(valueFont_);
        if (hintFont_) DeleteObject(hintFont_);
    }

    HWND hwnd() const { return hwnd_; }

    void set_global_restart(std::function<void()> action) {
        restartAction_ = std::move(action);
        if(!restartButton_) {
            restartButton_ = CreateWindowExW(0, L"BUTTON", L"GLOBAL RESTART",
                                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                             0, 0, 10, 10, hwnd_,
                                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRestartId)),
                                             GetModuleHandleW(nullptr), nullptr);
            if(!restartButton_) throw std::runtime_error("Global restart button CreateWindowEx failed");
            SendMessageW(restartButton_, WM_SETFONT, reinterpret_cast<WPARAM>(valueFont_), TRUE);
        }
        layout_action_buttons();
    }

    void set_global_play(std::function<void()> action) {
        playAction_ = std::move(action);
        if(!playButton_) {
            playButton_ = CreateWindowExW(0, L"BUTTON", L"GLOBAL PLAY",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                          0, 0, 10, 10, hwnd_,
                                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPlayId)),
                                          GetModuleHandleW(nullptr), nullptr);
            if(!playButton_) throw std::runtime_error("Global play button CreateWindowEx failed");
            SendMessageW(playButton_, WM_SETFONT, reinterpret_cast<WPARAM>(valueFont_), TRUE);
        }
        layout_action_buttons();
    }

    void add_section(std::wstring title) {
        Entry e{};
        e.isSection = true;
        e.section = std::move(title);
        entries_.push_back(std::move(e));
        recompute_content_height();
    }

    void add_slider(SliderSpec spec) {
        Entry e{};
        e.slider = std::move(spec);
        entries_.push_back(std::move(e));
        recompute_content_height();
    }

    void add_toggle(ToggleSpec spec) {
        Entry e{};
        e.isToggle = true;
        e.toggle = std::move(spec);
        entries_.push_back(std::move(e));
        recompute_content_height();
    }

    void refresh() {
        if (hwnd_ && IsWindow(hwnd_)) InvalidateRect(hwnd_, nullptr, FALSE);
    }

private:
    struct Entry {
        bool isSection = false;
        bool isToggle = false;
        std::wstring section;
        SliderSpec slider;
        ToggleSpec toggle;
    };

    static constexpr int kHeaderH = 112;
    static constexpr int kRestartId = 0x5A11;
    static constexpr int kPlayId = 0x5A12;
    static constexpr int kSectionH = 30;
    static constexpr int kSliderH = 72;
    static constexpr int kPadX = 18;
    static constexpr int kTrackLeft = 18;
    static constexpr int kTrackRight = 18;
    static constexpr int kTrackTopInRow = 42;
    static constexpr int kKnobRadius = 15;

    static const wchar_t* class_name() { return L"BKQR_GR_CUSTOM_SLIDER_PANEL"; }

    static HFONT make_font(int px, int weight) {
        return CreateFontW(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    }

    static void register_class() {
        static bool once = false;
        if (once) return;
        WNDCLASSW wc{};
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = &ControlPanel::s_proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = class_name();
        wc.hCursor = LoadCursor(nullptr, IDC_HAND);
        wc.hbrBackground = nullptr;
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("ControlPanel RegisterClass failed");
        once = true;
    }

    static LRESULT CALLBACK s_proc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
        ControlPanel* self = reinterpret_cast<ControlPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
            self = static_cast<ControlPanel*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if(!self) return DefWindowProcW(hwnd, msg, w, l);
        try {
            return self->proc(hwnd, msg, w, l);
        } catch(const std::exception& e) {
            std::fprintf(stderr,"CONTROL_PANEL_EXCEPTION=%s MSG=0x%04X\n",e.what(),(unsigned)msg);
            std::fflush(stderr);
            return 0;
        } catch(...) {
            std::fprintf(stderr,"CONTROL_PANEL_EXCEPTION=UNKNOWN MSG=0x%04X\n",(unsigned)msg);
            std::fflush(stderr);
            return 0;
        }
    }

    LRESULT proc(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
        switch (msg) {
            case WM_SIZE:
                width_ = LOWORD(l); height_ = HIWORD(l);
                clamp_scroll();
                layout_action_buttons();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            case WM_COMMAND:
                if(LOWORD(w)==kRestartId && HIWORD(w)==BN_CLICKED) {
                    if(restartAction_) restartAction_();
                    scrollY_=0;
                    active_=-1; hover_=-1;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if(LOWORD(w)==kPlayId && HIWORD(w)==BN_CLICKED) {
                    if(playAction_) playAction_();
                    active_=-1; hover_=-1;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                break;
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT:
                paint();
                return 0;
            case WM_LBUTTONDOWN:
                SetFocus(hwnd);
                on_mouse_down(GET_X_LPARAM(l), GET_Y_LPARAM(l), false);
                return 0;
            case WM_LBUTTONDBLCLK:
                on_mouse_down(GET_X_LPARAM(l), GET_Y_LPARAM(l), true);
                return 0;
            case WM_MOUSEMOVE:
                on_mouse_move(GET_X_LPARAM(l), GET_Y_LPARAM(l));
                return 0;
            case WM_LBUTTONUP:
                if (active_ >= 0) {
                    active_ = -1;
                    activeGrabOffsetPx_ = 0;
                    if(GetCapture()==hwnd) ReleaseCapture();
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            case WM_CAPTURECHANGED:
                active_=-1;
                activeGrabOffsetPx_=0;
                InvalidateRect(hwnd,nullptr,FALSE);
                return 0;
            case WM_NCDESTROY:
                SetWindowLongPtrW(hwnd,GWLP_USERDATA,0);
                if(hwnd_==hwnd) hwnd_=nullptr;
                return DefWindowProcW(hwnd,msg,w,l);
            case WM_MOUSELEAVE:
                hover_ = -1;
                trackingMouse_ = false;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            case WM_MOUSEWHEEL: {
                const int delta = GET_WHEEL_DELTA_WPARAM(w);
                scrollY_ -= (delta / WHEEL_DELTA) * 120;
                clamp_scroll();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            case WM_KEYDOWN:
                if (w == VK_HOME) { scrollY_ = 0; InvalidateRect(hwnd, nullptr, FALSE); return 0; }
                if (w == VK_END) { scrollY_ = std::max(0, contentHeight_ - height_); InvalidateRect(hwnd, nullptr, FALSE); return 0; }
                break;
        }
        return DefWindowProcW(hwnd, msg, w, l);
    }

    void layout_action_buttons() {
        const int bw=150, bh=30, gap=12, y=8;
        const int total=2*bw+gap;
        const int left=std::max(kPadX,(width_-total)/2);
        if(playButton_ && IsWindow(playButton_))
            MoveWindow(playButton_,left,y,bw,bh,TRUE);
        if(restartButton_ && IsWindow(restartButton_))
            MoveWindow(restartButton_,left+bw+gap,y,bw,bh,TRUE);
    }

    void recompute_content_height() {
        int h = kHeaderH + 10;
        for (const auto& e : entries_) h += e.isSection ? kSectionH : kSliderH;
        contentHeight_ = h + 20;
        clamp_scroll();
        if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void clamp_scroll() {
        scrollY_ = std::clamp(scrollY_, 0, std::max(0, contentHeight_ - height_));
    }

    int entry_y(int index) const {
        int y = kHeaderH - scrollY_;
        for (int i = 0; i < index; ++i) y += entries_[i].isSection ? kSectionH : kSliderH;
        return y;
    }

    int entry_at(int y) const {
        int cy = kHeaderH - scrollY_;
        for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
            const int h = entries_[i].isSection ? kSectionH : kSliderH;
            if (y >= cy && y < cy + h) return entries_[i].isSection ? -1 : i;
            cy += h;
        }
        return -1;
    }

    RECT track_rect_for(int entryIndex) const {
        const int y = entry_y(entryIndex);
        RECT r{ kTrackLeft, y + kTrackTopInRow,
                std::max(kTrackLeft + 20, width_ - kTrackRight), y + kTrackTopInRow + 8 };
        return r;
    }

    RECT toggle_rect_for(int entryIndex) const {
        const int y = entry_y(entryIndex);
        const int w = 82, h = 30;
        return RECT{ std::max(kPadX, width_-kPadX-w), y+34, width_-kPadX, y+34+h };
    }

    bool point_hits_toggle(int entryIndex, int x, int y) const {
        if(entryIndex<0 || entryIndex>=static_cast<int>(entries_.size())) return false;
        const auto& e=entries_[entryIndex];
        if(e.isSection || !e.isToggle) return false;
        RECT r=toggle_rect_for(entryIndex);
        return x>=r.left-6 && x<=r.right+6 && y>=r.top-6 && y<=r.bottom+6;
    }

    static double clamp01(double x) { return std::max(0.0, std::min(1.0, x)); }

    double to_norm(const SliderSpec& s, double value) const {
        value = std::clamp(value, s.minimum, s.maximum);
        if (s.scale == SliderScale::Logarithmic && s.minimum > 0.0 && s.maximum > s.minimum)
            return clamp01(std::log(value / s.minimum) / std::log(s.maximum / s.minimum));
        if (s.maximum <= s.minimum) return 0.0;
        return clamp01((value - s.minimum) / (s.maximum - s.minimum));
    }

    double from_norm(const SliderSpec& s, double t) const {
        t = clamp01(t);
        double v = 0.0;
        if (s.scale == SliderScale::Logarithmic && s.minimum > 0.0 && s.maximum > s.minimum)
            v = s.minimum * std::pow(s.maximum / s.minimum, t);
        else
            v = s.minimum + (s.maximum - s.minimum) * t;
        if (s.step > 0.0) v = s.minimum + std::round((v - s.minimum) / s.step) * s.step;
        return std::clamp(v, s.minimum, s.maximum);
    }

    int knob_x_for(int entryIndex) const {
        if (entryIndex < 0 || entryIndex >= static_cast<int>(entries_.size())) return kTrackLeft;
        const auto& e = entries_[entryIndex];
        if (e.isSection || e.isToggle) return kTrackLeft;
        RECT tr = track_rect_for(entryIndex);
        const double v = e.slider.get ? e.slider.get() : e.slider.defaultValue;
        const double t = to_norm(e.slider, v);
        return tr.left + static_cast<int>(std::lround(t * (tr.right-tr.left)));
    }

    bool point_hits_track_or_knob(int entryIndex, int x, int y, bool& onKnob) const {
        onKnob=false;
        if (entryIndex < 0 || entryIndex >= static_cast<int>(entries_.size()) || entries_[entryIndex].isSection || entries_[entryIndex].isToggle) return false;
        RECT tr=track_rect_for(entryIndex);
        const int cy=(tr.top+tr.bottom)/2;
        const int knobX=knob_x_for(entryIndex);
        const int dx=x-knobX, dy=y-cy;
        onKnob=(dx*dx+dy*dy)<=((kKnobRadius+7)*(kKnobRadius+7));
        const bool onTrack=(x>=tr.left-kKnobRadius && x<=tr.right+kKnobRadius && y>=cy-13 && y<=cy+13);
        return onKnob || onTrack;
    }

    void set_from_x(int entryIndex, int x) {
        if (entryIndex < 0 || entryIndex >= static_cast<int>(entries_.size())) return;
        auto& e = entries_[entryIndex];
        if (e.isSection || e.isToggle || !e.slider.set) return;
        RECT tr = track_rect_for(entryIndex);
        const double denom = std::max(1L, tr.right - tr.left);
        const double t = static_cast<double>(x - tr.left) / denom;
        e.slider.set(from_norm(e.slider, t));
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void on_mouse_down(int x, int y, bool doubleClick) {
        const int idx = entry_at(y);
        if (idx < 0) return;
        auto& e=entries_[idx];
        if(e.isToggle){
            if(!point_hits_toggle(idx,x,y)) return;
            const bool current=e.toggle.get?e.toggle.get():e.toggle.defaultValue;
            if(e.toggle.set) e.toggle.set(doubleClick?e.toggle.defaultValue:!current);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        bool onKnob=false;
        if(!point_hits_track_or_knob(idx,x,y,onKnob)) return;
        if (doubleClick) {
            auto& sl = entries_[idx].slider;
            if (sl.set) sl.set(sl.defaultValue);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        active_ = idx;
        activeGrabOffsetPx_ = onKnob ? (x-knob_x_for(idx)) : 0;
        SetCapture(hwnd_);
        if(!onKnob) set_from_x(idx, x);
    }

    void on_mouse_move(int x, int y) {
        if (!trackingMouse_) {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd_, 0 };
            TrackMouseEvent(&tme);
            trackingMouse_ = true;
        }
        if (active_ >= 0) {
            set_from_x(active_, x-activeGrabOffsetPx_);
            return;
        }
        const int h = entry_at(y);
        int newHover=-1;
        if(h>=0){
            if(entries_[h].isToggle){ if(point_hits_toggle(h,x,y)) newHover=h; }
            else { bool onKnob=false; if(point_hits_track_or_knob(h,x,y,onKnob)) newHover=h; }
        }
        if (newHover != hover_) {
            hover_ = newHover;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }

    static COLORREF rgb(BYTE r, BYTE g, BYTE b) { return RGB(r, g, b); }

    void paint() {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd_, &ps);
        RECT client{}; GetClientRect(hwnd_, &client);

        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = mem ? CreateCompatibleBitmap(hdc, (int)std::max<LONG>(1, client.right), (int)std::max<LONG>(1, client.bottom)) : nullptr;
        if(!mem || !bmp){
            if(bmp)DeleteObject(bmp);
            if(mem)DeleteDC(mem);
            FillRect(hdc,&client,(HBRUSH)GetStockObject(BLACK_BRUSH));
            EndPaint(hwnd_,&ps);
            return;
        }
        HGDIOBJ oldBmp = SelectObject(mem, bmp);

        HBRUSH bg = CreateSolidBrush(rgb(17, 20, 27));
        FillRect(mem, &client, bg);
        DeleteObject(bg);
        SetBkMode(mem, TRANSPARENT);

        RECT header{0,0,width_,kHeaderH};
        HBRUSH hb = CreateSolidBrush(rgb(24, 29, 39));
        FillRect(mem, &header, hb); DeleteObject(hb);

        const bool hasActions=(restartButton_&&IsWindow(restartButton_)) || (playButton_&&IsWindow(playButton_));
        SetTextColor(mem, rgb(238, 243, 250));
        SelectObject(mem, titleFont_);
        const int titleTop=hasActions?47:18;
        RECT tr{ kPadX, titleTop, width_-kPadX, titleTop+26 };
        DrawTextW(mem, titleText_.c_str(), -1, &tr, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        SelectObject(mem, hintFont_);
        SetTextColor(mem, rgb(143, 157, 178));
        const int hintTop=hasActions?74:48;
        RECT hr{ kPadX, hintTop, width_-kPadX, hintTop+26 };
        DrawTextW(mem, hintText_.c_str(), -1, &hr,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        IntersectClipRect(mem, 0, kHeaderH, width_, height_);
        int y = kHeaderH - scrollY_;
        for (int i=0; i<static_cast<int>(entries_.size()); ++i) {
            auto& e = entries_[i];
            if (e.isSection) {
                if (y + kSectionH >= kHeaderH && y <= height_) {
                    SelectObject(mem, valueFont_);
                    SetTextColor(mem, rgb(101, 195, 255));
                    RECT sr{ kPadX, y+6, width_-kPadX, y+kSectionH };
                    DrawTextW(mem, e.section.c_str(), -1, &sr, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                }
                y += kSectionH;
                continue;
            }
            if (y + kSliderH < kHeaderH || y > height_) { y += kSliderH; continue; }

            const bool hot = (i == hover_ || i == active_);
            if (hot) {
                RECT rr{ 8, y+2, width_-8, y+kSliderH-2 };
                HBRUSH rbr = CreateSolidBrush(rgb(24, 31, 42));
                HBRUSH old = static_cast<HBRUSH>(SelectObject(mem, rbr));
                HPEN pen = CreatePen(PS_SOLID, 1, rgb(48, 61, 79));
                HPEN oldPen = static_cast<HPEN>(SelectObject(mem, pen));
                RoundRect(mem, rr.left, rr.top, rr.right, rr.bottom, 12, 12);
                SelectObject(mem, old); SelectObject(mem, oldPen);
                DeleteObject(rbr); DeleteObject(pen);
            }

            if(e.isToggle){
                const bool value=e.toggle.get?e.toggle.get():e.toggle.defaultValue;
                SelectObject(mem,labelFont_);
                SetTextColor(mem,rgb(213,221,233));
                RECT lr{kPadX,y+8,width_-122,y+34};
                DrawTextW(mem,e.toggle.label.c_str(),-1,&lr,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);

                RECT sw=toggle_rect_for(i);
                HPEN nullPen=static_cast<HPEN>(GetStockObject(NULL_PEN));
                HPEN oldp=static_cast<HPEN>(SelectObject(mem,nullPen));
                HBRUSH rail=CreateSolidBrush(value?rgb(54,145,194):rgb(54,62,75));
                HBRUSH oldb=static_cast<HBRUSH>(SelectObject(mem,rail));
                RoundRect(mem,sw.left,sw.top,sw.right,sw.bottom,sw.bottom-sw.top,sw.bottom-sw.top);
                SelectObject(mem,oldb); DeleteObject(rail);
                const int rad=12, cy=(sw.top+sw.bottom)/2;
                const int cx=value?(sw.right-16):(sw.left+16);
                HBRUSH thumb=CreateSolidBrush(value?rgb(181,235,255):rgb(168,177,190));
                oldb=static_cast<HBRUSH>(SelectObject(mem,thumb));
                Ellipse(mem,cx-rad,cy-rad,cx+rad,cy+rad);
                SelectObject(mem,oldb); DeleteObject(thumb);
                SelectObject(mem,oldp);

                SelectObject(mem,valueFont_);
                SetTextColor(mem,value?rgb(124,213,255):rgb(173,191,212));
                std::wstring txt=value?e.toggle.onText:e.toggle.offText;
                RECT vr{static_cast<LONG>(kPadX),static_cast<LONG>(y+36),std::max<LONG>(static_cast<LONG>(kPadX+20),sw.left-10),static_cast<LONG>(y+65)};
                DrawTextW(mem,txt.c_str(),-1,&vr,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
                y += kSliderH;
                continue;
            }

            const double v = e.slider.get ? e.slider.get() : e.slider.defaultValue;
            const double t = to_norm(e.slider, v);
            std::wstring value = e.slider.format ? e.slider.format(v) : default_format(v);

            SelectObject(mem, labelFont_);
            SetTextColor(mem, rgb(213, 221, 233));
            RECT lr{ kPadX, y+5, width_-116, y+30 };
            DrawTextW(mem, e.slider.label.c_str(), -1, &lr, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            SelectObject(mem, valueFont_);
            SetTextColor(mem, hot ? rgb(124, 213, 255) : rgb(173, 191, 212));
            RECT vr{ width_-112, y+5, width_-kPadX, y+30 };
            DrawTextW(mem, value.c_str(), -1, &vr, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

            RECT track = track_rect_for(i);
            const int cy = (track.top + track.bottom) / 2;
            HBRUSH tb = CreateSolidBrush(rgb(54, 62, 75));
            HBRUSH oldb = static_cast<HBRUSH>(SelectObject(mem, tb));
            HPEN nullPen = static_cast<HPEN>(GetStockObject(NULL_PEN));
            HPEN oldp = static_cast<HPEN>(SelectObject(mem, nullPen));
            RoundRect(mem, track.left, cy-3, track.right, cy+3, 6, 6);
            SelectObject(mem, oldb); DeleteObject(tb);

            const int knobX = track.left + static_cast<int>(std::lround(t * (track.right-track.left)));
            HBRUSH ab = CreateSolidBrush(rgb(68, 178, 238));
            oldb = static_cast<HBRUSH>(SelectObject(mem, ab));
            RoundRect(mem, track.left, cy-3, knobX, cy+3, 6, 6);
            SelectObject(mem, oldb); DeleteObject(ab);

            HBRUSH shadow = CreateSolidBrush(rgb(8, 10, 14));
            oldb = static_cast<HBRUSH>(SelectObject(mem, shadow));
            Ellipse(mem, knobX-kKnobRadius-2, cy-kKnobRadius, knobX+kKnobRadius+2, cy+kKnobRadius+4);
            SelectObject(mem, oldb); DeleteObject(shadow);

            HBRUSH knob = CreateSolidBrush(active_==i ? rgb(166, 230, 255) : (hot ? rgb(112, 207, 250) : rgb(83, 188, 239)));
            oldb = static_cast<HBRUSH>(SelectObject(mem, knob));
            Ellipse(mem, knobX-kKnobRadius, cy-kKnobRadius, knobX+kKnobRadius, cy+kKnobRadius);
            SelectObject(mem, oldb); DeleteObject(knob);
            SelectObject(mem, oldp);

            y += kSliderH;
        }

        SelectClipRgn(mem, nullptr);

        // Scroll indicator.
        if (contentHeight_ > height_) {
            const int barTop = kHeaderH + 4;
            const int barBottom = height_ - 8;
            const int barH = std::max(28, (barBottom-barTop) * height_ / std::max(height_, contentHeight_));
            const int maxScroll = std::max(1, contentHeight_ - height_);
            const int y0 = barTop + (barBottom-barTop-barH) * scrollY_ / maxScroll;
            RECT sb{ width_-5, y0, width_-2, y0+barH };
            HBRUSH br = CreateSolidBrush(rgb(91, 110, 134)); FillRect(mem, &sb, br); DeleteObject(br);
        }

        BitBlt(hdc, 0, 0, client.right, client.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldBmp);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd_, &ps);
    }

    static std::wstring default_format(double v) {
        std::wostringstream os;
        os << std::fixed << std::setprecision(3) << v;
        return os.str();
    }

    HWND parent_{};
    HWND hwnd_{};
    HWND restartButton_{};
    HWND playButton_{};
    std::function<void()> restartAction_;
    std::function<void()> playAction_;
    int width_{};
    int height_{};
    int contentHeight_{};
    int scrollY_{};
    int active_ = -1;
    int activeGrabOffsetPx_ = 0;
    int hover_ = -1;
    bool trackingMouse_ = false;
    HFONT titleFont_{};
    HFONT labelFont_{};
    HFONT valueFont_{};
    HFONT hintFont_{};
    std::wstring titleText_;
    std::wstring hintText_;
    std::vector<Entry> entries_;
};

inline std::wstring format_fixed(double value, int decimals, const wchar_t* suffix=L"") {
    std::wostringstream os;
    os << std::fixed << std::setprecision(decimals) << value << suffix;
    return os.str();
}

inline std::wstring format_bool(double value, const wchar_t* onText=L"ON", const wchar_t* offText=L"OFF") {
    return value >= 0.5 ? onText : offText;
}

} // namespace bkqrgr_ui
