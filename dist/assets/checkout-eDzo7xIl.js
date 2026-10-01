import{k as A,l as R,r as W,B,o as P,S as F,A as z,P as C}from"./styles-DgLDVwk2.js";const h=1400,T=120;function _(b){let t;try{t=new A({canvas:b,antialias:!1,alpha:!0})}catch{return null}const y=window.innerWidth<860;t.setPixelRatio(Math.min(window.devicePixelRatio,y?1:1.5)),t.setSize(window.innerWidth,window.innerHeight,!1),t.setClearAlpha(0);const v=new R,a=new W(58,window.innerWidth/window.innerHeight,.1,400);a.position.set(0,0,0);const o=new Float32Array(h*3),s=new Float32Array(h*2);for(let e=0;e<h;e++){const i=T*Math.cbrt(Math.random()),S=Math.random()*Math.PI*2,m=Math.acos(1-2*Math.random());o[e*3]=i*Math.sin(m)*Math.cos(S),o[e*3+1]=i*Math.cos(m),o[e*3+2]=i*Math.sin(m)*Math.sin(S),s[e*2]=Math.random(),s[e*2+1]=Math.random()}const r=new B;r.setAttribute("position",new P(o,3)),r.setAttribute("aSeed",new P(s,2));const d={uTime:{value:0},uPixelRatio:{value:t.getPixelRatio()}},u=new F({uniforms:d,transparent:!0,depthWrite:!1,blending:z,vertexShader:`
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
    `}),w=new C(r,u);w.frustumCulled=!1,v.add(w);const p=()=>{t.setSize(window.innerWidth,window.innerHeight,!1),a.aspect=window.innerWidth/window.innerHeight,a.updateProjectionMatrix(),d.uPixelRatio.value=t.getPixelRatio()};window.addEventListener("resize",p);const f=window.matchMedia("(prefers-reduced-motion: reduce)").matches;let n=0,g=0,c=performance.now(),x=f?0:26;a.position.x=-16;const l=e=>{const i=Math.min((e-c)/1e3,.1);c=e,g+=i,d.uTime.value=g,x*=Math.exp(-1.6*i),a.position.x+=x*i,f||(a.position.x+=.35*i),t.render(v,a),n=requestAnimationFrame(l)},M=()=>{document.hidden?cancelAnimationFrame(n):(c=performance.now(),n=requestAnimationFrame(l))};return document.addEventListener("visibilitychange",M),n=requestAnimationFrame(l),{dispose(){cancelAnimationFrame(n),window.removeEventListener("resize",p),document.removeEventListener("visibilitychange",M),r.dispose(),u.dispose(),t.dispose()}}}export{_ as c};
