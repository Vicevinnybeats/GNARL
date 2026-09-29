import{W as C,S as B,P as E,B as F,a as A,b as z,A as L,c as T}from"./styles-C0cPIIU2.js";const v=1400,q=120;function H(d){let t;try{t=new C({canvas:d,antialias:!1,alpha:!0})}catch{return null}const W=window.innerWidth<860;t.setPixelRatio(Math.min(window.devicePixelRatio,W?1:1.5)),t.setSize(window.innerWidth,window.innerHeight,!1),t.setClearAlpha(0);const f=new B,a=new E(58,window.innerWidth/window.innerHeight,.1,400);a.position.set(0,0,0);const r=new Float32Array(v*3),c=new Float32Array(v*2);for(let e=0;e<v;e++){const n=q*Math.cbrt(Math.random()),b=Math.random()*Math.PI*2,u=Math.acos(1-2*Math.random());r[e*3]=n*Math.sin(u)*Math.cos(b),r[e*3+1]=n*Math.cos(u),r[e*3+2]=n*Math.sin(u)*Math.sin(b),c[e*2]=Math.random(),c[e*2+1]=Math.random()}const s=new F;s.setAttribute("position",new A(r,3)),s.setAttribute("aSeed",new A(c,2));const l={uTime:{value:0},uPixelRatio:{value:t.getPixelRatio()}},p=new z({uniforms:l,transparent:!0,depthWrite:!1,blending:L,vertexShader:`
      precision highp float;

      attribute vec2 aSeed;
      uniform float uTime;
      uniform float uPixelRatio;

      varying float vBright;
      varying float vWarm;

      void main() {
        vec4 mv = modelViewMatrix * vec4(position, 1.0);

        float b = aSeed.x * aSeed.x * aSeed.x * aSeed.x;
        b *= 0.65 + 0.35 * sin(uTime * 0.6 + aSeed.y * 90.0);

        vBright = b;
        vWarm = aSeed.y;

        gl_PointSize = (0.6 + b * 3.0) * uPixelRatio * (90.0 / max(-mv.z, 1.0));
        gl_Position = projectionMatrix * mv;
      }
    `,fragmentShader:`
      precision highp float;

      varying float vBright;
      varying float vWarm;

      void main() {
        vec2 d = gl_PointCoord - 0.5;
        float r = length(d);
        if (r > 0.5) discard;

        float alpha = smoothstep(0.5, 0.05, r);

        vec3 cool = vec3(0.62, 0.78, 1.00);
        vec3 warm = vec3(1.00, 0.82, 0.58);
        vec3 colour = mix(cool, warm, smoothstep(0.55, 1.0, vWarm));

        gl_FragColor = vec4(colour, alpha * (0.16 + vBright * 0.9));
      }
    `}),w=new T(s,p);w.frustumCulled=!1,f.add(w);const g=()=>{t.setSize(window.innerWidth,window.innerHeight,!1),a.aspect=window.innerWidth/window.innerHeight,a.updateProjectionMatrix(),l.uPixelRatio.value=t.getPixelRatio()};window.addEventListener("resize",g);const x=window.matchMedia("(prefers-reduced-motion: reduce)").matches;let o=0,S=0,m=performance.now(),y=x?0:26;a.position.x=-16;const h=e=>{const n=Math.min((e-m)/1e3,.1);m=e,S+=n,l.uTime.value=S,y*=Math.exp(-1.6*n),a.position.x+=y*n,x||(a.position.x+=.35*n),t.render(f,a),o=requestAnimationFrame(h)},M=()=>{document.hidden?cancelAnimationFrame(o):(m=performance.now(),o=requestAnimationFrame(h))};return document.addEventListener("visibilitychange",M),o=requestAnimationFrame(h),{dispose(){cancelAnimationFrame(o),window.removeEventListener("resize",g),document.removeEventListener("visibilitychange",M),s.dispose(),p.dispose(),t.dispose()}}}const P=document.querySelector("#stage");P&&H(P);const i=document.querySelector("#pay"),_=document.querySelector("#pay-note"),R=i?.dataset.paymentLink?.trim()??"";i&&R.length>0?(i.href=R,i.removeAttribute("aria-disabled"),i.textContent="PAY €49 — SECURE CHECKOUT",i.rel="noreferrer",_?.remove()):i&&(i.addEventListener("click",d=>d.preventDefault()),i.tabIndex=-1);
