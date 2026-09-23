// BKQR_GR_OPTICAL_DYNAMICS_DX12_v1_6_4_4
// Renderer-grade binary black-hole optical dynamics.
// Geometry: adiabatically moving two-puncture isotropic optical metric.
// Dynamics: 3PN binding energy + 3.5PN flux balance, NR-calibrated remnant/ringdown.
// Claim boundary: reduced-order PN/NR model, not a full BSSN/Z4c numerical-relativity spacetime.

cbuffer FrameCB : register(b0)
{
    uint4  Header0;    // magic, protocol, flags, render mode
    uint4  Header1;    // width, height, maxSteps, frameId
    float4 Camera0;    // camera xyz, fovY
    float4 CamRight;
    float4 CamUp;
    float4 CamForward;
    float4 Render0;    // exposure, sim time, rate, sim dt
    float4 Binary0;    // m1, m2, d_now, phi_now
    float4 Binary1;    // eta, omega_now, d_dot_now, RR enabled
    float4 Binary2;    // object1 radius, capture u, max ray travel, sky scale OR detector half-height
    float4 Quality0;   // integrator quality, baseSPP, extraSPP, stride
    float4 Quality1;   // maxStep, minStep, criticalGrad, object2 radius
    float4 Visual0;    // merger blend, GW power, field gain OR source size, time since transition
    float4 Visual1;    // plane overlay/detector distance, scale/source distance, remnant/system mass, remnant spin
    float4 Reserved0;  // source size/r0, beam softness/E_rad, show mirrored source sphere, source sphere radius
    float4 Reserved1;  // omega_QNM220, tau_QNM220, transition r, transition omega
};

RWTexture2D<float4> Output : register(u0);

static const float PI = 3.14159265358979323846;
static const uint FLAG_SKY = 1u;
static const uint FLAG_FIELD_DEBUG = 2u;
static const uint FLAG_CAPTURE_DEBUG = 4u;

static const uint RENDER_MODE_SKY = 0u;
static const uint RENDER_MODE_FIELD = 1u;
static const uint RENDER_MODE_CAPTURE = 2u;
static const uint RENDER_MODE_POINT_DETECTOR = 3u;
static const uint RENDER_MODE_CYLINDER_DETECTOR = 4u;

struct BinaryAtTime {
    float d;
    float phi;
    float blend;
    float remnantMass;
    float3 x1;
    float3 x2;
};

struct FieldSample {
    float u;
    float N;
    float3 gradLogN;
    float r1;
    float r2;
    float r0;
    float blend;
    float3 x1;
    float3 x2;
};

struct TraceResult {
    float captured;
    float escaped;
    float captureId;
    float3 exitDir;
    float maxU;
    float maxGrad;
    float travel;
    float3 capturePos;
};

struct SourcePlaneResult {
    float reachedPlane;
    float captured;
    float3 hitPos;
    float maxU;
    float maxGrad;
    float travel;
};

float hash11(float x)
{
    return frac(sin(x*12.9898 + 78.233)*43758.5453);
}
float hash31(float3 p)
{
    return frac(sin(dot(p,float3(127.1,311.7,74.7)))*43758.5453123);
}
float2 sample_jitter(uint2 tid, uint sampleIndex, uint frameId)
{
    float seed=float(tid.x*1973u + tid.y*9277u + sampleIndex*26699u + frameId*31847u);
    return float2(hash11(seed+0.17),hash11(seed+19.73));
}

// Reconstruct a causal, retarded approximation to the CPU trajectory.  The
// exact CPU PN history is not stored per ray; before the strong-field switch we
// use the leading balance law as a backward local reconstruction.  During the
// merger we use the same Gaussian collapse envelope as the CPU and recover the
// pre-transition binary when the optical travel time reaches behind the switch.
BinaryAtTime binary_retarded(float tau)
{
    BinaryAtTime b;
    float m1=Binary0.x, m2=Binary0.y;
    float M=max(1e-8,m1+m2);
    float dnow=max(1e-7,Binary0.z);
    float phinow=Binary0.w;
    float eta=max(1e-8,Binary1.x);
    float omegaNow=Binary1.y;
    bool rr=(Binary1.w>0.5);
    float currentBlend=saturate(Visual0.x);
    float elapsed=max(0.0,Visual0.w);
    float qnmTau=max(1e-5,Reserved1.y);
    float tauR=max(2.0*M,0.55*qnmTau);
    float dtr=max(1e-6,Reserved1.z);
    float omtr=Reserved1.w;
    float sgn=(omegaNow>=0.0)?1.0:-1.0;
    float d=dnow, phi=phinow, blend=currentBlend;

    if(rr && currentBlend>1e-6 && elapsed>0.0)
    {
        if(tau<=elapsed)
        {
            float past=max(0.0,elapsed-tau);
            float up=past/tauR;
            float e=exp(-0.5*up*up);
            d=dtr*e;
            blend=saturate(1.0-e);
            // Local phase reconstruction is deliberately first order in
            // retarded time; geometric collapse and causal binary/remnant
            // selection are the important ray-time effects here.
            phi=phinow-omegaNow*tau;
        }
        else
        {
            float pre=tau-elapsed;
            blend=0.0;
            float K=(64.0/5.0)*eta*M*M*M;
            float d4=pow(dtr,4.0)+4.0*K*pre;
            d=pow(max(d4,1e-8),0.25);
            float phiTr=phinow-omegaNow*elapsed;
            float dphase=(2.0*sqrt(M)/(5.0*K))*(pow(d,2.5)-pow(dtr,2.5));
            phi=phiTr-sgn*dphase;
        }
    }
    else if(rr && tau>0.0)
    {
        float K=(64.0/5.0)*eta*M*M*M;
        float d4=pow(dnow,4.0)+4.0*K*tau;
        d=pow(max(d4,1e-8),0.25);
        float dphase=(2.0*sqrt(M)/(5.0*K))*(pow(d,2.5)-pow(dnow,2.5));
        phi=phinow-sgn*dphase;
        blend=0.0;
    }
    else
    {
        phi=phinow-omegaNow*tau;
    }

    float3 er=float3(cos(phi),sin(phi),0.0);
    b.d=d; b.phi=phi; b.blend=blend; b.remnantMass=max(1e-6,Visual1.z);
    b.x1=((m2/M)*d)*er;
    b.x2=(-(m1/M)*d)*er;
    return b;
}

FieldSample field_sample(float3 x, float tau)
{
    BinaryAtTime b=binary_retarded(tau);
    FieldSample f;
    float3 d1=x-b.x1;
    float3 d2=x-b.x2;
    float3 d0=x;
    float r1=max(1e-4,length(d1));
    float r2=max(1e-4,length(d2));
    float r0=max(1e-4,length(d0));
    float m1=Binary0.x, m2=Binary0.y;
    float blend=saturate(b.blend);
    float uBinary=m1/(2.0*r1)+m2/(2.0*r2);
    float uRem=b.remnantMass/(2.0*r0);
    float u=lerp(uBinary,uRem,blend);
    float oneMinus=max(1e-5,1.0-u);
    float psi=1.0+u;
    float N=(psi*psi*psi)/oneMinus;
    float3 gradBinary=-(m1/(2.0*r1*r1*r1))*d1 -(m2/(2.0*r2*r2*r2))*d2;
    float3 gradRem=-(b.remnantMass/(2.0*r0*r0*r0))*d0;
    float3 gradU=lerp(gradBinary,gradRem,blend);
    float fac=3.0/psi + 1.0/oneMinus;
    f.u=u;
    f.N=N;
    f.gradLogN=fac*gradU;
    f.r1=r1; f.r2=r2; f.r0=r0; f.blend=blend; f.x1=b.x1; f.x2=b.x2;
    return f;
}

float3 optical_accel(float3 nhat, FieldSample f)
{
    return f.gradLogN-dot(nhat,f.gradLogN)*nhat;
}

float effective_individual_optical_radius(float mSelf,float mOther,float separation,float blend,float captureU)
{
    float oneMinus=saturate(1.0-blend);
    float d=max(1e-6,separation);
    float companionU=oneMinus*mOther/(2.0*d);
    float denom=max(0.05,captureU-companionU);
    float localIso=mSelf/(2.0*denom);
    return localIso*sqrt(oneMinus);
}

TraceResult trace_ray(float3 x0, float3 n0)
{
    TraceResult tr=(TraceResult)0;
    float3 x=x0;
    float3 n=normalize(n0);
    float tau=0.0;
    float maxU=0.0, maxGrad=0.0;
    float captureU=clamp(Binary2.y,0.70,0.995);
    float maxTravel=max(10.0,Binary2.z);
    uint maxSteps=min(Header1.z,12000u);
    float iq=clamp(Quality0.x,0.5,4.0);
    float maxStep=clamp(Quality1.x,0.002,0.20);
    float minStep=clamp(Quality1.y,0.0002,maxStep);
    float rCam=length(x0);

    [loop]
    for(uint step=0u; step<maxSteps; ++step)
    {
        FieldSample f=field_sample(x,tau);
        maxU=max(maxU,f.u);
        float gmag=length(f.gradLogN);
        maxGrad=max(maxGrad,gmag);

        float sepNow=max(1e-6,length(f.x1-f.x2));
        float rObj1=effective_individual_optical_radius(Binary0.x,Binary0.y,sepNow,f.blend,captureU);
        float rObj2=effective_individual_optical_radius(Binary0.y,Binary0.x,sepNow,f.blend,captureU);
        bool hitObj1=(f.blend<0.999 && f.r1<=rObj1);
        bool hitObj2=(f.blend<0.999 && f.r2<=rObj2);
        if(f.u>=captureU || hitObj1 || hitObj2)
        {
            tr.captured=1.0;
            tr.captureId=(f.blend>0.55)?3.0:(hitObj1?1.0:(hitObj2?2.0:((f.r1<=f.r2)?1.0:2.0)));
            tr.capturePos=x;
            tr.exitDir=n;
            tr.maxU=maxU; tr.maxGrad=maxGrad; tr.travel=tau;
            return tr;
        }

        if(step>12u && length(x)>rCam*1.06 && dot(x,n)>0.0)
        {
            tr.escaped=1.0; tr.exitDir=n; tr.maxU=maxU; tr.maxGrad=maxGrad; tr.travel=tau;
            return tr;
        }
        if(tau>maxTravel || length(x)>max(4.0*rCam,120.0))
        {
            tr.escaped=1.0; tr.exitDir=n; tr.maxU=maxU; tr.maxGrad=maxGrad; tr.travel=tau;
            return tr;
        }

        float localScale=lerp(min(f.r1,f.r2),f.r0,f.blend);
        float dsGeom=0.18*max(0.03,localScale)/iq;
        float dsBend=maxStep/(1.0+0.30*gmag*iq);
        float ds=clamp(min(maxStep,min(dsGeom,dsBend)),minStep,maxStep);

        // Midpoint update of Fermat ray equation.
        float3 a0=optical_accel(n,f);
        float3 nmid=normalize(n+0.5*ds*a0);
        float3 xmid=x+0.5*ds*n;
        float taumid=tau+0.5*ds*min(f.N,128.0);
        FieldSample fm=field_sample(xmid,taumid);
        float3 amid=optical_accel(nmid,fm);
        x += ds*nmid;
        n = normalize(n+ds*amid);
        tau += ds*min(fm.N,128.0);
    }

    tr.exitDir=n; tr.maxU=maxU; tr.maxGrad=maxGrad; tr.travel=tau;
    return tr;
}

SourcePlaneResult trace_to_source_plane(float3 x0, float3 n0, float3 planePoint, float3 planeNormal)
{
    SourcePlaneResult tr=(SourcePlaneResult)0;
    float3 x=x0;
    float3 n=normalize(n0);
    float3 planeN=normalize(planeNormal);
    float tau=0.0;
    float maxU=0.0, maxGrad=0.0;
    float captureU=clamp(Binary2.y,0.70,0.995);
    float maxTravel=max(10.0,Binary2.z);
    uint maxSteps=min(Header1.z,12000u);
    float iq=clamp(Quality0.x,0.5,4.0);
    float maxStep=clamp(Quality1.x,0.002,0.20);
    float minStep=clamp(Quality1.y,0.0002,maxStep);

    [loop]
    for(uint step=0u; step<maxSteps; ++step)
    {
        FieldSample f=field_sample(x,tau);
        maxU=max(maxU,f.u);
        float gmag=length(f.gradLogN);
        maxGrad=max(maxGrad,gmag);

        float sepNow=max(1e-6,length(f.x1-f.x2));
        float rObj1=effective_individual_optical_radius(Binary0.x,Binary0.y,sepNow,f.blend,captureU);
        float rObj2=effective_individual_optical_radius(Binary0.y,Binary0.x,sepNow,f.blend,captureU);
        bool hitObj1=(f.blend<0.999 && f.r1<=rObj1);
        bool hitObj2=(f.blend<0.999 && f.r2<=rObj2);
        if(f.u>=captureU || hitObj1 || hitObj2)
        {
            tr.captured=1.0;
            tr.hitPos=x;
            tr.maxU=maxU; tr.maxGrad=maxGrad; tr.travel=tau;
            return tr;
        }

        float localScale=lerp(min(f.r1,f.r2),f.r0,f.blend);
        float dsGeom=0.18*max(0.03,localScale)/iq;
        float dsBend=maxStep/(1.0+0.30*gmag*iq);
        float ds=clamp(min(maxStep,min(dsGeom,dsBend)),minStep,maxStep);

        float3 xPrev=x;
        float tauPrev=tau;
        float3 a0=optical_accel(n,f);
        float3 nmid=normalize(n+0.5*ds*a0);
        float3 xmid=x+0.5*ds*n;
        float taumid=tau+0.5*ds*min(f.N,128.0);
        FieldSample fm=field_sample(xmid,taumid);
        float3 amid=optical_accel(nmid,fm);
        float3 xNext=x + ds*nmid;
        float3 nNext=normalize(n+ds*amid);
        float tauNext=tau + ds*min(fm.N,128.0);

        float sdPrev=dot(xPrev-planePoint, planeN);
        float sdNext=dot(xNext-planePoint, planeN);
        if(sdPrev*sdNext<=0.0)
        {
            float denom=sdPrev-sdNext;
            float a=(abs(denom)>1e-6)?saturate(sdPrev/denom):1.0;
            tr.reachedPlane=1.0;
            tr.hitPos=lerp(xPrev,xNext,a);
            tr.maxU=maxU; tr.maxGrad=maxGrad; tr.travel=lerp(tauPrev,tauNext,a);
            return tr;
        }

        x=xNext;
        n=nNext;
        tau=tauNext;

        if(tau>maxTravel || length(x)>max(220.0,6.0*length(x0)))
        {
            tr.hitPos=x;
            tr.maxU=maxU; tr.maxGrad=maxGrad; tr.travel=tau;
            return tr;
        }
    }

    tr.hitPos=x; tr.maxU=maxU; tr.maxGrad=maxGrad; tr.travel=tau;
    return tr;
}

float3 sky_color(float3 d)
{
    // User-requested clean background: no star field, no textured sky.
    return float3(0.0,0.0,0.0);
}

float3 capture_debug(float id, float maxU)
{
    float edge=saturate((maxU-0.65)/0.35);
    if(id<1.5) return lerp(float3(0.01,0.0,0.0),float3(0.75,0.08,0.02),edge);
    if(id<2.5) return lerp(float3(0.0,0.0,0.01),float3(0.02,0.25,0.85),edge);
    return lerp(float3(0.005,0.0,0.008),float3(0.55,0.18,0.85),edge);
}

struct ShadeResult { float3 color; float critical; };

float source_sphere_profile(float rho, float radius)
{
    radius=max(radius,1e-4);
    float rr=saturate(rho/radius);
    float limb=sqrt(saturate(1.0-rr*rr));
    float edge=1.0-smoothstep(radius, radius+0.15*radius+0.02, rho);
    return edge*(0.30+0.70*limb);
}

ShadeResult shade_detector_sample(float2 uv, uint mode)
{
    uint W=Header1.x, H=Header1.y;
    float2 ndc=float2(2.0*uv.x-1.0,1.0-2.0*uv.y);
    float aspect=(float)W/(float)H;

    float3 detCenter=Camera0.xyz;
    // In detector modes the camera FOV now really controls the physical screen
    // aperture. Binary2.w remains a dimensionless user scale around the
    // historical default value 10, so every camera knob has a visible effect.
    float detectorScale=max(0.05,Binary2.w/10.0);
    float detectorHalf=max(0.1,length(detCenter)*tan(0.5*Camera0.w)*detectorScale);
    float3 axis=normalize(CamForward.xyz);
    float3 detRight=normalize(CamRight.xyz);
    float3 detUp=normalize(CamUp.xyz);
    float3 detPos=detCenter + ndc.x*aspect*detectorHalf*detRight + ndc.y*detectorHalf*detUp;

    // Mirrored source: reflect the detector center through the binary barycenter.
    float3 srcCenter=-detCenter;
    float3 srcNormal=axis;
    float3 dir0=(mode==RENDER_MODE_POINT_DETECTOR) ? normalize(srcCenter-detPos) : axis;

    SourcePlaneResult tr=trace_to_source_plane(detPos,dir0,srcCenter,srcNormal);
    ShadeResult sr;
    sr.critical=(tr.maxGrad>Quality1.z || tr.captured>0.5)?1.0:0.0;

    float showSphere=Reserved0.z;
    float intensity=0.0;
    if(tr.reachedPlane>0.5 && tr.captured<0.5)
    {
        float3 dv=tr.hitPos-srcCenter;
        float u=dot(dv,detRight);
        float v=dot(dv,detUp);
        float rho=length(float2(u,v));
        if(showSphere>0.5)
        {
            float sphereRadius=max(0.01,Reserved0.w);
            intensity=source_sphere_profile(rho,sphereRadius);
        }
        else if(mode==RENDER_MODE_POINT_DETECTOR)
        {
            float sigma=max(0.01,Reserved0.x);
            intensity=exp(-0.5*(rho*rho)/(sigma*sigma));
        }
        else
        {
            float radius=max(0.01,Reserved0.x);
            float softness=max(0.005,Reserved0.y);
            intensity=1.0-smoothstep(radius,radius+softness,rho);
        }
    }

    float caustic=saturate(0.22*tr.maxGrad);
    float shadow=(tr.captured>0.5)?1.0:0.0;
    float3 base=(showSphere>0.5)?float3(1.00,0.94,0.78):((mode==RENDER_MODE_POINT_DETECTOR)?float3(1.0,0.96,0.86):float3(0.96,0.84,0.62));
    float3 radiance=intensity*(0.06+0.94*(1.0+0.65*caustic))*base;
    radiance += intensity*caustic*float3(0.08,0.18,0.35);
    if(shadow>0.5) radiance=0.0.xxx;

    // Faint 2D centerpoint/crosshair marking the barycentric optical axis.
    float centerDot=exp(-220.0*dot(ndc,ndc));
    float cross=(1.0-smoothstep(0.0,0.004,abs(ndc.x))) + (1.0-smoothstep(0.0,0.004,abs(ndc.y)));
    radiance += 0.06*centerDot*float3(0.55,0.9,1.0);
    radiance += 0.02*cross*float3(0.28,0.5,0.62);

    float exposure=max(0.01,Render0.x);
    float3 mapped=1.0-exp(-exposure*max(float3(0,0,0),radiance));
    sr.color=saturate(pow(mapped,1.0/2.2));
    return sr;
}

ShadeResult shade_sample(float2 uv)
{
    uint mode=Header0.w;
    if(mode==RENDER_MODE_POINT_DETECTOR || mode==RENDER_MODE_CYLINDER_DETECTOR)
        return shade_detector_sample(uv,mode);

    uint W=Header1.x, H=Header1.y;
    float2 ndc=float2(2.0*uv.x-1.0,1.0-2.0*uv.y);
    float aspect=(float)W/(float)H;
    float tanHalf=tan(0.5*Camera0.w);
    float3 dir=normalize(CamForward.xyz + ndc.x*aspect*tanHalf*CamRight.xyz + ndc.y*tanHalf*CamUp.xyz);

    TraceResult tr=trace_ray(Camera0.xyz,dir);
    ShadeResult sr;
    sr.critical=(tr.maxGrad>Quality1.z || (tr.maxU>0.70 && tr.captured<0.5))?1.0:0.0;

    uint flags=Header0.z;
    float3 radiance;
    if((flags & FLAG_FIELD_DEBUG)!=0u)
    {
        float u=saturate(tr.maxU);
        float bend=saturate(0.15*tr.maxGrad*Visual0.z);
        radiance=float3(u,bend,1.0-u)*0.9;
        if(tr.captured>0.5) radiance*=0.15;
    }
    else if((flags & FLAG_CAPTURE_DEBUG)!=0u)
    {
        radiance=(tr.captured>0.5)?capture_debug(tr.captureId,tr.maxU):0.35*sky_color(tr.exitDir);
    }
    else
    {
        radiance=(tr.captured>0.5)?float3(0,0,0):sky_color(tr.exitDir);
        if(tr.captured<0.5 && Reserved0.z>0.5)
        {
            float3 srcCenter=-Camera0.xyz;
            float3 srcNormal=normalize(CamForward.xyz);
            SourcePlaneResult src=trace_to_source_plane(Camera0.xyz,dir,srcCenter,srcNormal);
            if(src.reachedPlane>0.5 && src.captured<0.5)
            {
                float3 dv=src.hitPos-srcCenter;
                float su=dot(dv,CamRight.xyz);
                float sv=dot(dv,CamUp.xyz);
                float rho=length(float2(su,sv));
                float sphereGlow=source_sphere_profile(rho,max(0.01,Reserved0.w));
                radiance += sphereGlow*float3(1.00,0.94,0.78);
            }
        }
    }

    float edgeGlow=(tr.captured<0.5)?0.015*saturate((tr.maxU-0.55)/0.30):0.0;
    radiance += edgeGlow*float3(0.65,0.75,1.0);

    float exposure=max(0.01,Render0.x);
    float3 mapped=1.0-exp(-exposure*max(float3(0,0,0),radiance));
    sr.color=saturate(pow(mapped,1.0/2.2));
    return sr;
}



float3 orbitalInset(uint2 px,float3 baseColor){
    if(Header0.w>=RENDER_MODE_POINT_DETECTOR || Visual1.x<0.5) return baseColor;
    uint W=Header1.x,H=Header1.y; float size=190.0*max(0.5,Visual1.y); float2 lo=float2(18.0,H-size-18.0), hi=lo+size;
    float2 p=(float2)px; if(any(p<lo)||any(p>hi)) return baseColor;
    float2 uv=(p-lo)/size; float2 q=2.0*uv-1.0; q.y=-q.y;
    BinaryAtTime b=binary_retarded(0.0); float scaleD=max(Reserved1.z,1e-3); float2 p1=b.x1.xy/(0.7*scaleD), p2=b.x2.xy/(0.7*scaleD);
    float bg=0.025+0.05*(1.0-smoothstep(0.0,0.02,abs(length(q)-0.72)));
    float fade=1.0-saturate(b.blend);
    float captureU=clamp(Binary2.y,0.70,0.995);
    float rObj1=effective_individual_optical_radius(Binary0.x,Binary0.y,max(1e-6,b.d),b.blend,captureU);
    float rObj2=effective_individual_optical_radius(Binary0.y,Binary0.x,max(1e-6,b.d),b.blend,captureU);
    float qr1=max(0.018,rObj1/(0.7*scaleD));
    float qr2=max(0.018,rObj2/(0.7*scaleD));
    float g1=fade*exp(-dot(q-p1,q-p1)/max(1e-5,qr1*qr1));
    float g2=fade*exp(-dot(q-p2,q-p2)/max(1e-5,qr2*qr2));
    float remRadius=max(0.018,(Visual1.z/(2.0*captureU))/(0.7*scaleD));
    float rem=saturate(b.blend)*exp(-dot(q,q)/max(1e-5,remRadius*remRadius));
    float axis=(1.0-smoothstep(0.0,0.012,abs(q.x)))+(1.0-smoothstep(0.0,0.012,abs(q.y)));
    // Outgoing rings are telemetry for radiated GW power only; they do not
    // feed back into the optical geodesic equation.
    float rr=length(q);
    float waveAmp=saturate(45.0*sqrt(max(0.0,Visual0.y)));
    float wave=waveAmp*(0.5+0.5*sin(42.0*rr-2.0*Reserved1.x*Render0.y))*exp(-1.7*rr);
    float3 c=lerp(baseColor,float3(bg,bg*1.25,bg*1.7)+0.08*axis+g1*float3(1.0,.25,.08)+g2*float3(.08,.4,1.0)+rem*float3(.65,.24,.9)+wave*float3(.14,.18,.24),0.86);
    return c;
}

[numthreads(8,8,1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint W=Header1.x, H=Header1.y;
    if(Header0.x!=0x42475142u || Header0.y!=0x00016000u)
    {
        if(tid.x<W && tid.y<H) Output[tid.xy]=float4(1.0,0.0,1.0,1.0);
        return;
    }

    uint stride=clamp((uint)round(Quality0.w),1u,4u);
    uint lowW=(W+stride-1u)/stride;
    uint lowH=(H+stride-1u)/stride;
    if(tid.x>=lowW || tid.y>=lowH) return;

    uint2 base=tid.xy*stride;
    uint spp=clamp((uint)round(Quality0.y),1u,8u);
    uint extra=clamp((uint)round(Quality0.z),0u,12u);
    float3 sum=float3(0,0,0);
    uint used=0u;
    float critical=0.0;

    [loop]
    for(uint sidx=0u;sidx<spp;++sidx)
    {
        float2 q=(spp==1u)?float2(0.5,0.5):sample_jitter(tid.xy,sidx,Header1.w);
        float2 pix=min(float2(W-1u,H-1u),float2(base)+q*(float)stride);
        ShadeResult sr=shade_sample((pix+0.5)/float2(W,H));
        sum+=sr.color; used++; critical=max(critical,sr.critical);
    }
    if(critical>0.5 && extra>0u)
    {
        [loop]
        for(uint e=0u;e<extra;++e)
        {
            float2 q=sample_jitter(tid.xy,spp+e,Header1.w);
            float2 pix=min(float2(W-1u,H-1u),float2(base)+q*(float)stride);
            ShadeResult sr=shade_sample((pix+0.5)/float2(W,H));
            sum+=sr.color; used++;
        }
    }
    float3 outc=sum/max(1.0,(float)used); outc=orbitalInset(base,outc);
    [loop]
    for(uint oy=0u;oy<stride;++oy)
    [loop]
    for(uint ox=0u;ox<stride;++ox)
    {
        uint2 p=base+uint2(ox,oy);
        if(p.x<W && p.y<H) Output[p]=float4(outc,1.0);
    }
}
