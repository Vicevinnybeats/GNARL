import{T as Qt,a as ze,b as Rt,L as Jt,c as he,F as bt,M as V,V as C,C as F,d as W,S as J,e as Zt,P as $t,D as en,f as Re,g as T,I as tn,Q as St,h as nn,O as Mt,i as _t,j as sn,B as O,k as on,l as Lt,N as rn,m as an,n as cn,o as Ve,p as It,R as Ke,q as ln,r as un,s as dn,t as De,u as hn,v as Nt,w as Ct,x as se,y as fn,z as fe,A as pn,E as be,G as Ot,H as mn,J as gn,K as We,U as Ee,W as Ft,X as P,Y as An,Z as Tn,_ as wn,$ as xn,a0 as vn,a1 as Pt,a2 as yn,a3 as dt,a4 as ht,a5 as ft,a6 as pt,a7 as mt,a8 as En,a9 as Rn,aa as Dt,ab as Xe,ac as bn,ad as Sn,ae as Mn,af as _n,ag as Ln,ah as ve,ai as ye,aj as In,ak as Nn,al as Cn,am as On,an as Fn,ao as Pn,ap as Dn,aq as kn}from"./index-D0Fm2jXZ.js";function gt(d,t){if(t===Qt)return console.warn("THREE.BufferGeometryUtils.toTrianglesDrawMode(): Geometry already defined as triangles."),d;if(t===ze||t===Rt){let e=d.getIndex();if(e===null){const o=[],a=d.getAttribute("position");if(a!==void 0){for(let i=0;i<a.count;i++)o.push(i);d.setIndex(o),e=d.getIndex()}else return console.error("THREE.BufferGeometryUtils.toTrianglesDrawMode(): Undefined position attribute. Processing not possible."),d}const r=e.count-2,n=[];if(t===ze)for(let o=1;o<=r;o++)n.push(e.getX(0)),n.push(e.getX(o)),n.push(e.getX(o+1));else for(let o=0;o<r;o++)o%2===0?(n.push(e.getX(o)),n.push(e.getX(o+1)),n.push(e.getX(o+2))):(n.push(e.getX(o+2)),n.push(e.getX(o+1)),n.push(e.getX(o)));n.length/3!==r&&console.error("THREE.BufferGeometryUtils.toTrianglesDrawMode(): Unable to generate correct amount of triangles.");const s=d.clone();return s.setIndex(n),s.clearGroups(),s}else return console.error("THREE.BufferGeometryUtils.toTrianglesDrawMode(): Unknown draw mode:",t),d}class Hn extends Jt{constructor(t){super(t),this.dracoLoader=null,this.ktx2Loader=null,this.meshoptDecoder=null,this.pluginCallbacks=[],this.register(function(e){return new zn(e)}),this.register(function(e){return new Vn(e)}),this.register(function(e){return new $n(e)}),this.register(function(e){return new es(e)}),this.register(function(e){return new ts(e)}),this.register(function(e){return new Wn(e)}),this.register(function(e){return new Xn(e)}),this.register(function(e){return new qn(e)}),this.register(function(e){return new Yn(e)}),this.register(function(e){return new jn(e)}),this.register(function(e){return new Qn(e)}),this.register(function(e){return new Kn(e)}),this.register(function(e){return new Zn(e)}),this.register(function(e){return new Jn(e)}),this.register(function(e){return new Gn(e)}),this.register(function(e){return new ns(e)}),this.register(function(e){return new ss(e)})}load(t,e,r,n){const s=this;let o;if(this.resourcePath!=="")o=this.resourcePath;else if(this.path!==""){const c=he.extractUrlBase(t);o=he.resolveURL(c,this.path)}else o=he.extractUrlBase(t);this.manager.itemStart(t);const a=function(c){n?n(c):console.error(c),s.manager.itemError(t),s.manager.itemEnd(t)},i=new bt(this.manager);i.setPath(this.path),i.setResponseType("arraybuffer"),i.setRequestHeader(this.requestHeader),i.setWithCredentials(this.withCredentials),i.load(t,function(c){try{s.parse(c,o,function(u){e(u),s.manager.itemEnd(t)},a)}catch(u){a(u)}},r,a)}setDRACOLoader(t){return this.dracoLoader=t,this}setKTX2Loader(t){return this.ktx2Loader=t,this}setMeshoptDecoder(t){return this.meshoptDecoder=t,this}register(t){return this.pluginCallbacks.indexOf(t)===-1&&this.pluginCallbacks.push(t),this}unregister(t){return this.pluginCallbacks.indexOf(t)!==-1&&this.pluginCallbacks.splice(this.pluginCallbacks.indexOf(t),1),this}parse(t,e,r,n){let s;const o={},a={},i=new TextDecoder;if(typeof t=="string")s=JSON.parse(t);else if(t instanceof ArrayBuffer)if(i.decode(new Uint8Array(t,0,4))===kt){try{o[w.KHR_BINARY_GLTF]=new os(t)}catch(l){n&&n(l);return}s=JSON.parse(o[w.KHR_BINARY_GLTF].content)}else s=JSON.parse(i.decode(t));else s=t;if(s.asset===void 0||s.asset.version[0]<2){n&&n(new Error("THREE.GLTFLoader: Unsupported asset. glTF versions >=2.0 are supported."));return}const c=new As(s,{path:e||this.resourcePath||"",crossOrigin:this.crossOrigin,requestHeader:this.requestHeader,manager:this.manager,ktx2Loader:this.ktx2Loader,meshoptDecoder:this.meshoptDecoder});c.fileLoader.setRequestHeader(this.requestHeader);for(let u=0;u<this.pluginCallbacks.length;u++){const l=this.pluginCallbacks[u](c);l.name||console.error("THREE.GLTFLoader: Invalid plugin found: missing name"),a[l.name]=l,o[l.name]=!0}if(s.extensionsUsed)for(let u=0;u<s.extensionsUsed.length;++u){const l=s.extensionsUsed[u],h=s.extensionsRequired||[];switch(l){case w.KHR_MATERIALS_UNLIT:o[l]=new Un;break;case w.KHR_DRACO_MESH_COMPRESSION:o[l]=new rs(s,this.dracoLoader);break;case w.KHR_TEXTURE_TRANSFORM:o[l]=new is;break;case w.KHR_MESH_QUANTIZATION:o[l]=new as;break;default:h.indexOf(l)>=0&&a[l]===void 0&&console.warn('THREE.GLTFLoader: Unknown extension "'+l+'".')}}c.setExtensions(o),c.setPlugins(a),c.parse(r,n)}parseAsync(t,e){const r=this;return new Promise(function(n,s){r.parse(t,e,n,s)})}}function Bn(){let d={};return{get:function(t){return d[t]},add:function(t,e){d[t]=e},remove:function(t){delete d[t]},removeAll:function(){d={}}}}const w={KHR_BINARY_GLTF:"KHR_binary_glTF",KHR_DRACO_MESH_COMPRESSION:"KHR_draco_mesh_compression",KHR_LIGHTS_PUNCTUAL:"KHR_lights_punctual",KHR_MATERIALS_CLEARCOAT:"KHR_materials_clearcoat",KHR_MATERIALS_DISPERSION:"KHR_materials_dispersion",KHR_MATERIALS_IOR:"KHR_materials_ior",KHR_MATERIALS_SHEEN:"KHR_materials_sheen",KHR_MATERIALS_SPECULAR:"KHR_materials_specular",KHR_MATERIALS_TRANSMISSION:"KHR_materials_transmission",KHR_MATERIALS_IRIDESCENCE:"KHR_materials_iridescence",KHR_MATERIALS_ANISOTROPY:"KHR_materials_anisotropy",KHR_MATERIALS_UNLIT:"KHR_materials_unlit",KHR_MATERIALS_VOLUME:"KHR_materials_volume",KHR_TEXTURE_BASISU:"KHR_texture_basisu",KHR_TEXTURE_TRANSFORM:"KHR_texture_transform",KHR_MESH_QUANTIZATION:"KHR_mesh_quantization",KHR_MATERIALS_EMISSIVE_STRENGTH:"KHR_materials_emissive_strength",EXT_MATERIALS_BUMP:"EXT_materials_bump",EXT_TEXTURE_WEBP:"EXT_texture_webp",EXT_TEXTURE_AVIF:"EXT_texture_avif",EXT_MESHOPT_COMPRESSION:"EXT_meshopt_compression",EXT_MESH_GPU_INSTANCING:"EXT_mesh_gpu_instancing"};class Gn{constructor(t){this.parser=t,this.name=w.KHR_LIGHTS_PUNCTUAL,this.cache={refs:{},uses:{}}}_markDefs(){const t=this.parser,e=this.parser.json.nodes||[];for(let r=0,n=e.length;r<n;r++){const s=e[r];s.extensions&&s.extensions[this.name]&&s.extensions[this.name].light!==void 0&&t._addNodeRef(this.cache,s.extensions[this.name].light)}}_loadLight(t){const e=this.parser,r="light:"+t;let n=e.cache.get(r);if(n)return n;const s=e.json,i=((s.extensions&&s.extensions[this.name]||{}).lights||[])[t];let c;const u=new F(16777215);i.color!==void 0&&u.setRGB(i.color[0],i.color[1],i.color[2],W);const l=i.range!==void 0?i.range:0;switch(i.type){case"directional":c=new en(u),c.target.position.set(0,0,-1),c.add(c.target);break;case"point":c=new $t(u),c.distance=l;break;case"spot":c=new Zt(u),c.distance=l,i.spot=i.spot||{},i.spot.innerConeAngle=i.spot.innerConeAngle!==void 0?i.spot.innerConeAngle:0,i.spot.outerConeAngle=i.spot.outerConeAngle!==void 0?i.spot.outerConeAngle:Math.PI/4,c.angle=i.spot.outerConeAngle,c.penumbra=1-i.spot.innerConeAngle/i.spot.outerConeAngle,c.target.position.set(0,0,-1),c.add(c.target);break;default:throw new Error("THREE.GLTFLoader: Unexpected light type: "+i.type)}return c.position.set(0,0,0),c.decay=2,K(c,i),i.intensity!==void 0&&(c.intensity=i.intensity),c.name=e.createUniqueName(i.name||"light_"+t),n=Promise.resolve(c),e.cache.add(r,n),n}getDependency(t,e){if(t==="light")return this._loadLight(e)}createNodeAttachment(t){const e=this,r=this.parser,s=r.json.nodes[t],a=(s.extensions&&s.extensions[this.name]||{}).light;return a===void 0?null:this._loadLight(a).then(function(i){return r._getNodeRef(e.cache,a,i)})}}class Un{constructor(){this.name=w.KHR_MATERIALS_UNLIT}getMaterialType(){return se}extendParams(t,e,r){const n=[];t.color=new F(1,1,1),t.opacity=1;const s=e.pbrMetallicRoughness;if(s){if(Array.isArray(s.baseColorFactor)){const o=s.baseColorFactor;t.color.setRGB(o[0],o[1],o[2],W),t.opacity=o[3]}s.baseColorTexture!==void 0&&n.push(r.assignTexture(t,"map",s.baseColorTexture,J))}return Promise.all(n)}}class jn{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_EMISSIVE_STRENGTH}extendMaterialParams(t,e){const n=this.parser.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=n.extensions[this.name].emissiveStrength;return s!==void 0&&(e.emissiveIntensity=s),Promise.resolve()}}class zn{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_CLEARCOAT}getMaterialType(t){const r=this.parser.json.materials[t];return!r.extensions||!r.extensions[this.name]?null:V}extendMaterialParams(t,e){const r=this.parser,n=r.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[],o=n.extensions[this.name];if(o.clearcoatFactor!==void 0&&(e.clearcoat=o.clearcoatFactor),o.clearcoatTexture!==void 0&&s.push(r.assignTexture(e,"clearcoatMap",o.clearcoatTexture)),o.clearcoatRoughnessFactor!==void 0&&(e.clearcoatRoughness=o.clearcoatRoughnessFactor),o.clearcoatRoughnessTexture!==void 0&&s.push(r.assignTexture(e,"clearcoatRoughnessMap",o.clearcoatRoughnessTexture)),o.clearcoatNormalTexture!==void 0&&(s.push(r.assignTexture(e,"clearcoatNormalMap",o.clearcoatNormalTexture)),o.clearcoatNormalTexture.scale!==void 0)){const a=o.clearcoatNormalTexture.scale;e.clearcoatNormalScale=new C(a,a)}return Promise.all(s)}}class Vn{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_DISPERSION}getMaterialType(t){const r=this.parser.json.materials[t];return!r.extensions||!r.extensions[this.name]?null:V}extendMaterialParams(t,e){const n=this.parser.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=n.extensions[this.name];return e.dispersion=s.dispersion!==void 0?s.dispersion:0,Promise.resolve()}}class Kn{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_IRIDESCENCE}getMaterialType(t){const r=this.parser.json.materials[t];return!r.extensions||!r.extensions[this.name]?null:V}extendMaterialParams(t,e){const r=this.parser,n=r.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[],o=n.extensions[this.name];return o.iridescenceFactor!==void 0&&(e.iridescence=o.iridescenceFactor),o.iridescenceTexture!==void 0&&s.push(r.assignTexture(e,"iridescenceMap",o.iridescenceTexture)),o.iridescenceIor!==void 0&&(e.iridescenceIOR=o.iridescenceIor),e.iridescenceThicknessRange===void 0&&(e.iridescenceThicknessRange=[100,400]),o.iridescenceThicknessMinimum!==void 0&&(e.iridescenceThicknessRange[0]=o.iridescenceThicknessMinimum),o.iridescenceThicknessMaximum!==void 0&&(e.iridescenceThicknessRange[1]=o.iridescenceThicknessMaximum),o.iridescenceThicknessTexture!==void 0&&s.push(r.assignTexture(e,"iridescenceThicknessMap",o.iridescenceThicknessTexture)),Promise.all(s)}}class Wn{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_SHEEN}getMaterialType(t){const r=this.parser.json.materials[t];return!r.extensions||!r.extensions[this.name]?null:V}extendMaterialParams(t,e){const r=this.parser,n=r.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[];e.sheenColor=new F(0,0,0),e.sheenRoughness=0,e.sheen=1;const o=n.extensions[this.name];if(o.sheenColorFactor!==void 0){const a=o.sheenColorFactor;e.sheenColor.setRGB(a[0],a[1],a[2],W)}return o.sheenRoughnessFactor!==void 0&&(e.sheenRoughness=o.sheenRoughnessFactor),o.sheenColorTexture!==void 0&&s.push(r.assignTexture(e,"sheenColorMap",o.sheenColorTexture,J)),o.sheenRoughnessTexture!==void 0&&s.push(r.assignTexture(e,"sheenRoughnessMap",o.sheenRoughnessTexture)),Promise.all(s)}}class Xn{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_TRANSMISSION}getMaterialType(t){const r=this.parser.json.materials[t];return!r.extensions||!r.extensions[this.name]?null:V}extendMaterialParams(t,e){const r=this.parser,n=r.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[],o=n.extensions[this.name];return o.transmissionFactor!==void 0&&(e.transmission=o.transmissionFactor),o.transmissionTexture!==void 0&&s.push(r.assignTexture(e,"transmissionMap",o.transmissionTexture)),Promise.all(s)}}class qn{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_VOLUME}getMaterialType(t){const r=this.parser.json.materials[t];return!r.extensions||!r.extensions[this.name]?null:V}extendMaterialParams(t,e){const r=this.parser,n=r.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[],o=n.extensions[this.name];e.thickness=o.thicknessFactor!==void 0?o.thicknessFactor:0,o.thicknessTexture!==void 0&&s.push(r.assignTexture(e,"thicknessMap",o.thicknessTexture)),e.attenuationDistance=o.attenuationDistance||1/0;const a=o.attenuationColor||[1,1,1];return e.attenuationColor=new F().setRGB(a[0],a[1],a[2],W),Promise.all(s)}}class Yn{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_IOR}getMaterialType(t){const r=this.parser.json.materials[t];return!r.extensions||!r.extensions[this.name]?null:V}extendMaterialParams(t,e){const n=this.parser.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=n.extensions[this.name];return e.ior=s.ior!==void 0?s.ior:1.5,Promise.resolve()}}class Qn{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_SPECULAR}getMaterialType(t){const r=this.parser.json.materials[t];return!r.extensions||!r.extensions[this.name]?null:V}extendMaterialParams(t,e){const r=this.parser,n=r.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[],o=n.extensions[this.name];e.specularIntensity=o.specularFactor!==void 0?o.specularFactor:1,o.specularTexture!==void 0&&s.push(r.assignTexture(e,"specularIntensityMap",o.specularTexture));const a=o.specularColorFactor||[1,1,1];return e.specularColor=new F().setRGB(a[0],a[1],a[2],W),o.specularColorTexture!==void 0&&s.push(r.assignTexture(e,"specularColorMap",o.specularColorTexture,J)),Promise.all(s)}}class Jn{constructor(t){this.parser=t,this.name=w.EXT_MATERIALS_BUMP}getMaterialType(t){const r=this.parser.json.materials[t];return!r.extensions||!r.extensions[this.name]?null:V}extendMaterialParams(t,e){const r=this.parser,n=r.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[],o=n.extensions[this.name];return e.bumpScale=o.bumpFactor!==void 0?o.bumpFactor:1,o.bumpTexture!==void 0&&s.push(r.assignTexture(e,"bumpMap",o.bumpTexture)),Promise.all(s)}}class Zn{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_ANISOTROPY}getMaterialType(t){const r=this.parser.json.materials[t];return!r.extensions||!r.extensions[this.name]?null:V}extendMaterialParams(t,e){const r=this.parser,n=r.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[],o=n.extensions[this.name];return o.anisotropyStrength!==void 0&&(e.anisotropy=o.anisotropyStrength),o.anisotropyRotation!==void 0&&(e.anisotropyRotation=o.anisotropyRotation),o.anisotropyTexture!==void 0&&s.push(r.assignTexture(e,"anisotropyMap",o.anisotropyTexture)),Promise.all(s)}}class $n{constructor(t){this.parser=t,this.name=w.KHR_TEXTURE_BASISU}loadTexture(t){const e=this.parser,r=e.json,n=r.textures[t];if(!n.extensions||!n.extensions[this.name])return null;const s=n.extensions[this.name],o=e.options.ktx2Loader;if(!o){if(r.extensionsRequired&&r.extensionsRequired.indexOf(this.name)>=0)throw new Error("THREE.GLTFLoader: setKTX2Loader must be called before loading KTX2 textures");return null}return e.loadTextureImage(t,s.source,o)}}class es{constructor(t){this.parser=t,this.name=w.EXT_TEXTURE_WEBP,this.isSupported=null}loadTexture(t){const e=this.name,r=this.parser,n=r.json,s=n.textures[t];if(!s.extensions||!s.extensions[e])return null;const o=s.extensions[e],a=n.images[o.source];let i=r.textureLoader;if(a.uri){const c=r.options.manager.getHandler(a.uri);c!==null&&(i=c)}return this.detectSupport().then(function(c){if(c)return r.loadTextureImage(t,o.source,i);if(n.extensionsRequired&&n.extensionsRequired.indexOf(e)>=0)throw new Error("THREE.GLTFLoader: WebP required by asset but unsupported.");return r.loadTexture(t)})}detectSupport(){return this.isSupported||(this.isSupported=new Promise(function(t){const e=new Image;e.src="data:image/webp;base64,UklGRiIAAABXRUJQVlA4IBYAAAAwAQCdASoBAAEADsD+JaQAA3AAAAAA",e.onload=e.onerror=function(){t(e.height===1)}})),this.isSupported}}class ts{constructor(t){this.parser=t,this.name=w.EXT_TEXTURE_AVIF,this.isSupported=null}loadTexture(t){const e=this.name,r=this.parser,n=r.json,s=n.textures[t];if(!s.extensions||!s.extensions[e])return null;const o=s.extensions[e],a=n.images[o.source];let i=r.textureLoader;if(a.uri){const c=r.options.manager.getHandler(a.uri);c!==null&&(i=c)}return this.detectSupport().then(function(c){if(c)return r.loadTextureImage(t,o.source,i);if(n.extensionsRequired&&n.extensionsRequired.indexOf(e)>=0)throw new Error("THREE.GLTFLoader: AVIF required by asset but unsupported.");return r.loadTexture(t)})}detectSupport(){return this.isSupported||(this.isSupported=new Promise(function(t){const e=new Image;e.src="data:image/avif;base64,AAAAIGZ0eXBhdmlmAAAAAGF2aWZtaWYxbWlhZk1BMUIAAADybWV0YQAAAAAAAAAoaGRscgAAAAAAAAAAcGljdAAAAAAAAAAAAAAAAGxpYmF2aWYAAAAADnBpdG0AAAAAAAEAAAAeaWxvYwAAAABEAAABAAEAAAABAAABGgAAABcAAAAoaWluZgAAAAAAAQAAABppbmZlAgAAAAABAABhdjAxQ29sb3IAAAAAamlwcnAAAABLaXBjbwAAABRpc3BlAAAAAAAAAAEAAAABAAAAEHBpeGkAAAAAAwgICAAAAAxhdjFDgQAMAAAAABNjb2xybmNseAACAAIABoAAAAAXaXBtYQAAAAAAAAABAAEEAQKDBAAAAB9tZGF0EgAKCBgABogQEDQgMgkQAAAAB8dSLfI=",e.onload=e.onerror=function(){t(e.height===1)}})),this.isSupported}}class ns{constructor(t){this.name=w.EXT_MESHOPT_COMPRESSION,this.parser=t}loadBufferView(t){const e=this.parser.json,r=e.bufferViews[t];if(r.extensions&&r.extensions[this.name]){const n=r.extensions[this.name],s=this.parser.getDependency("buffer",n.buffer),o=this.parser.options.meshoptDecoder;if(!o||!o.supported){if(e.extensionsRequired&&e.extensionsRequired.indexOf(this.name)>=0)throw new Error("THREE.GLTFLoader: setMeshoptDecoder must be called before loading compressed files");return null}return s.then(function(a){const i=n.byteOffset||0,c=n.byteLength||0,u=n.count,l=n.byteStride,h=new Uint8Array(a,i,c);return o.decodeGltfBufferAsync?o.decodeGltfBufferAsync(u,l,h,n.mode,n.filter).then(function(f){return f.buffer}):o.ready.then(function(){const f=new ArrayBuffer(u*l);return o.decodeGltfBuffer(new Uint8Array(f),u,l,h,n.mode,n.filter),f})})}else return null}}class ss{constructor(t){this.name=w.EXT_MESH_GPU_INSTANCING,this.parser=t}createNodeMesh(t){const e=this.parser.json,r=e.nodes[t];if(!r.extensions||!r.extensions[this.name]||r.mesh===void 0)return null;const n=e.meshes[r.mesh];for(const c of n.primitives)if(c.mode!==k.TRIANGLES&&c.mode!==k.TRIANGLE_STRIP&&c.mode!==k.TRIANGLE_FAN&&c.mode!==void 0)return null;const o=r.extensions[this.name].attributes,a=[],i={};for(const c in o)a.push(this.parser.getDependency("accessor",o[c]).then(u=>(i[c]=u,i[c])));return a.length<1?null:(a.push(this.parser.createNodeMesh(t)),Promise.all(a).then(c=>{const u=c.pop(),l=u.isGroup?u.children:[u],h=c[0].count,f=[];for(const m of l){const x=new Re,p=new T,g=new St,y=new T(1,1,1),R=new tn(m.geometry,m.material,h);for(let v=0;v<h;v++)i.TRANSLATION&&p.fromBufferAttribute(i.TRANSLATION,v),i.ROTATION&&g.fromBufferAttribute(i.ROTATION,v),i.SCALE&&y.fromBufferAttribute(i.SCALE,v),R.setMatrixAt(v,x.compose(p,g,y));for(const v in i)if(v==="_COLOR_0"){const S=i[v];R.instanceColor=new nn(S.array,S.itemSize,S.normalized)}else v!=="TRANSLATION"&&v!=="ROTATION"&&v!=="SCALE"&&m.geometry.setAttribute(v,i[v]);Mt.prototype.copy.call(R,m),this.parser.assignFinalMaterial(R),f.push(R)}return u.isGroup?(u.clear(),u.add(...f),u):f[0]}))}}const kt="glTF",ue=12,At={JSON:1313821514,BIN:5130562};class os{constructor(t){this.name=w.KHR_BINARY_GLTF,this.content=null,this.body=null;const e=new DataView(t,0,ue),r=new TextDecoder;if(this.header={magic:r.decode(new Uint8Array(t.slice(0,4))),version:e.getUint32(4,!0),length:e.getUint32(8,!0)},this.header.magic!==kt)throw new Error("THREE.GLTFLoader: Unsupported glTF-Binary header.");if(this.header.version<2)throw new Error("THREE.GLTFLoader: Legacy binary file detected.");const n=this.header.length-ue,s=new DataView(t,ue);let o=0;for(;o<n;){const a=s.getUint32(o,!0);o+=4;const i=s.getUint32(o,!0);if(o+=4,i===At.JSON){const c=new Uint8Array(t,ue+o,a);this.content=r.decode(c)}else if(i===At.BIN){const c=ue+o;this.body=t.slice(c,c+a)}o+=a}if(this.content===null)throw new Error("THREE.GLTFLoader: JSON content not found.")}}class rs{constructor(t,e){if(!e)throw new Error("THREE.GLTFLoader: No DRACOLoader instance provided.");this.name=w.KHR_DRACO_MESH_COMPRESSION,this.json=t,this.dracoLoader=e,this.dracoLoader.preload()}decodePrimitive(t,e){const r=this.json,n=this.dracoLoader,s=t.extensions[this.name].bufferView,o=t.extensions[this.name].attributes,a={},i={},c={};for(const u in o){const l=qe[u]||u.toLowerCase();a[l]=o[u]}for(const u in t.attributes){const l=qe[u]||u.toLowerCase();if(o[u]!==void 0){const h=r.accessors[t.attributes[u]],f=oe[h.componentType];c[l]=f.name,i[l]=h.normalized===!0}}return e.getDependency("bufferView",s).then(function(u){return new Promise(function(l,h){n.decodeDracoFile(u,function(f){for(const m in f.attributes){const x=f.attributes[m],p=i[m];p!==void 0&&(x.normalized=p)}l(f)},a,c,W,h)})})}}class is{constructor(){this.name=w.KHR_TEXTURE_TRANSFORM}extendTexture(t,e){return(e.texCoord===void 0||e.texCoord===t.channel)&&e.offset===void 0&&e.rotation===void 0&&e.scale===void 0||(t=t.clone(),e.texCoord!==void 0&&(t.channel=e.texCoord),e.offset!==void 0&&t.offset.fromArray(e.offset),e.rotation!==void 0&&(t.rotation=e.rotation),e.scale!==void 0&&t.repeat.fromArray(e.scale),t.needsUpdate=!0),t}}class as{constructor(){this.name=w.KHR_MESH_QUANTIZATION}}class Ht extends Rn{constructor(t,e,r,n){super(t,e,r,n)}copySampleValue_(t){const e=this.resultBuffer,r=this.sampleValues,n=this.valueSize,s=t*n*3+n;for(let o=0;o!==n;o++)e[o]=r[s+o];return e}interpolate_(t,e,r,n){const s=this.resultBuffer,o=this.sampleValues,a=this.valueSize,i=a*2,c=a*3,u=n-e,l=(r-e)/u,h=l*l,f=h*l,m=t*c,x=m-c,p=-2*f+3*h,g=f-h,y=1-p,R=g-h+l;for(let v=0;v!==a;v++){const S=o[x+v+a],M=o[x+v+i]*u,b=o[m+v+a],I=o[m+v]*u;s[v]=y*S+R*M+p*b+g*I}return s}}const cs=new St;class ls extends Ht{interpolate_(t,e,r,n){const s=super.interpolate_(t,e,r,n);return cs.fromArray(s).normalize().toArray(s),s}}const k={POINTS:0,LINES:1,LINE_LOOP:2,LINE_STRIP:3,TRIANGLES:4,TRIANGLE_STRIP:5,TRIANGLE_FAN:6},oe={5120:Int8Array,5121:Uint8Array,5122:Int16Array,5123:Uint16Array,5125:Uint32Array,5126:Float32Array},Tt={9728:It,9729:Ve,9984:cn,9985:an,9986:rn,9987:Lt},wt={33071:un,33648:ln,10497:Ke},ke={SCALAR:1,VEC2:2,VEC3:3,VEC4:4,MAT2:4,MAT3:9,MAT4:16},qe={POSITION:"position",NORMAL:"normal",TANGENT:"tangent",TEXCOORD_0:"uv",TEXCOORD_1:"uv1",TEXCOORD_2:"uv2",TEXCOORD_3:"uv3",COLOR_0:"color",WEIGHTS_0:"skinWeight",JOINTS_0:"skinIndex"},q={scale:"scale",translation:"position",rotation:"quaternion",weights:"morphTargetInfluences"},us={CUBICSPLINE:void 0,LINEAR:Pt,STEP:vn},He={OPAQUE:"OPAQUE",MASK:"MASK",BLEND:"BLEND"};function ds(d){return d.DefaultMaterial===void 0&&(d.DefaultMaterial=new Nt({color:16777215,emissive:0,metalness:1,roughness:1,transparent:!1,depthTest:!0,side:En})),d.DefaultMaterial}function Q(d,t,e){for(const r in e.extensions)d[r]===void 0&&(t.userData.gltfExtensions=t.userData.gltfExtensions||{},t.userData.gltfExtensions[r]=e.extensions[r])}function K(d,t){t.extras!==void 0&&(typeof t.extras=="object"?Object.assign(d.userData,t.extras):console.warn("THREE.GLTFLoader: Ignoring primitive type .extras, "+t.extras))}function hs(d,t,e){let r=!1,n=!1,s=!1;for(let c=0,u=t.length;c<u;c++){const l=t[c];if(l.POSITION!==void 0&&(r=!0),l.NORMAL!==void 0&&(n=!0),l.COLOR_0!==void 0&&(s=!0),r&&n&&s)break}if(!r&&!n&&!s)return Promise.resolve(d);const o=[],a=[],i=[];for(let c=0,u=t.length;c<u;c++){const l=t[c];if(r){const h=l.POSITION!==void 0?e.getDependency("accessor",l.POSITION):d.attributes.position;o.push(h)}if(n){const h=l.NORMAL!==void 0?e.getDependency("accessor",l.NORMAL):d.attributes.normal;a.push(h)}if(s){const h=l.COLOR_0!==void 0?e.getDependency("accessor",l.COLOR_0):d.attributes.color;i.push(h)}}return Promise.all([Promise.all(o),Promise.all(a),Promise.all(i)]).then(function(c){const u=c[0],l=c[1],h=c[2];return r&&(d.morphAttributes.position=u),n&&(d.morphAttributes.normal=l),s&&(d.morphAttributes.color=h),d.morphTargetsRelative=!0,d})}function fs(d,t){if(d.updateMorphTargets(),t.weights!==void 0)for(let e=0,r=t.weights.length;e<r;e++)d.morphTargetInfluences[e]=t.weights[e];if(t.extras&&Array.isArray(t.extras.targetNames)){const e=t.extras.targetNames;if(d.morphTargetInfluences.length===e.length){d.morphTargetDictionary={};for(let r=0,n=e.length;r<n;r++)d.morphTargetDictionary[e[r]]=r}else console.warn("THREE.GLTFLoader: Invalid extras.targetNames length. Ignoring names.")}}function ps(d){let t;const e=d.extensions&&d.extensions[w.KHR_DRACO_MESH_COMPRESSION];if(e?t="draco:"+e.bufferView+":"+e.indices+":"+Be(e.attributes):t=d.indices+":"+Be(d.attributes)+":"+d.mode,d.targets!==void 0)for(let r=0,n=d.targets.length;r<n;r++)t+=":"+Be(d.targets[r]);return t}function Be(d){let t="";const e=Object.keys(d).sort();for(let r=0,n=e.length;r<n;r++)t+=e[r]+":"+d[e[r]]+";";return t}function Ye(d){switch(d){case Int8Array:return 1/127;case Uint8Array:return 1/255;case Int16Array:return 1/32767;case Uint16Array:return 1/65535;default:throw new Error("THREE.GLTFLoader: Unsupported normalized accessor component type.")}}function ms(d){return d.search(/\.jpe?g($|\?)/i)>0||d.search(/^data\:image\/jpeg/)===0?"image/jpeg":d.search(/\.webp($|\?)/i)>0||d.search(/^data\:image\/webp/)===0?"image/webp":d.search(/\.ktx2($|\?)/i)>0||d.search(/^data\:image\/ktx2/)===0?"image/ktx2":"image/png"}const gs=new Re;class As{constructor(t={},e={}){this.json=t,this.extensions={},this.plugins={},this.options=e,this.cache=new Bn,this.associations=new Map,this.primitiveCache={},this.nodeCache={},this.meshCache={refs:{},uses:{}},this.cameraCache={refs:{},uses:{}},this.lightCache={refs:{},uses:{}},this.sourceCache={},this.textureCache={},this.nodeNamesUsed={};let r=!1,n=-1,s=!1,o=-1;if(typeof navigator<"u"){const a=navigator.userAgent;r=/^((?!chrome|android).)*safari/i.test(a)===!0;const i=a.match(/Version\/(\d+)/);n=r&&i?parseInt(i[1],10):-1,s=a.indexOf("Firefox")>-1,o=s?a.match(/Firefox\/([0-9]+)\./)[1]:-1}typeof createImageBitmap>"u"||r&&n<17||s&&o<98?this.textureLoader=new _t(this.options.manager):this.textureLoader=new sn(this.options.manager),this.textureLoader.setCrossOrigin(this.options.crossOrigin),this.textureLoader.setRequestHeader(this.options.requestHeader),this.fileLoader=new bt(this.options.manager),this.fileLoader.setResponseType("arraybuffer"),this.options.crossOrigin==="use-credentials"&&this.fileLoader.setWithCredentials(!0)}setExtensions(t){this.extensions=t}setPlugins(t){this.plugins=t}parse(t,e){const r=this,n=this.json,s=this.extensions;this.cache.removeAll(),this.nodeCache={},this._invokeAll(function(o){return o._markDefs&&o._markDefs()}),Promise.all(this._invokeAll(function(o){return o.beforeRoot&&o.beforeRoot()})).then(function(){return Promise.all([r.getDependencies("scene"),r.getDependencies("animation"),r.getDependencies("camera")])}).then(function(o){const a={scene:o[0][n.scene||0],scenes:o[0],animations:o[1],cameras:o[2],asset:n.asset,parser:r,userData:{}};return Q(s,a,n),K(a,n),Promise.all(r._invokeAll(function(i){return i.afterRoot&&i.afterRoot(a)})).then(function(){for(const i of a.scenes)i.updateMatrixWorld();t(a)})}).catch(e)}_markDefs(){const t=this.json.nodes||[],e=this.json.skins||[],r=this.json.meshes||[];for(let n=0,s=e.length;n<s;n++){const o=e[n].joints;for(let a=0,i=o.length;a<i;a++)t[o[a]].isBone=!0}for(let n=0,s=t.length;n<s;n++){const o=t[n];o.mesh!==void 0&&(this._addNodeRef(this.meshCache,o.mesh),o.skin!==void 0&&(r[o.mesh].isSkinnedMesh=!0)),o.camera!==void 0&&this._addNodeRef(this.cameraCache,o.camera)}}_addNodeRef(t,e){e!==void 0&&(t.refs[e]===void 0&&(t.refs[e]=t.uses[e]=0),t.refs[e]++)}_getNodeRef(t,e,r){if(t.refs[e]<=1)return r;const n=r.clone(),s=(o,a)=>{const i=this.associations.get(o);i!=null&&this.associations.set(a,i);for(const[c,u]of o.children.entries())s(u,a.children[c])};return s(r,n),n.name+="_instance_"+t.uses[e]++,n}_invokeOne(t){const e=Object.values(this.plugins);e.push(this);for(let r=0;r<e.length;r++){const n=t(e[r]);if(n)return n}return null}_invokeAll(t){const e=Object.values(this.plugins);e.unshift(this);const r=[];for(let n=0;n<e.length;n++){const s=t(e[n]);s&&r.push(s)}return r}getDependency(t,e){const r=t+":"+e;let n=this.cache.get(r);if(!n){switch(t){case"scene":n=this.loadScene(e);break;case"node":n=this._invokeOne(function(s){return s.loadNode&&s.loadNode(e)});break;case"mesh":n=this._invokeOne(function(s){return s.loadMesh&&s.loadMesh(e)});break;case"accessor":n=this.loadAccessor(e);break;case"bufferView":n=this._invokeOne(function(s){return s.loadBufferView&&s.loadBufferView(e)});break;case"buffer":n=this.loadBuffer(e);break;case"material":n=this._invokeOne(function(s){return s.loadMaterial&&s.loadMaterial(e)});break;case"texture":n=this._invokeOne(function(s){return s.loadTexture&&s.loadTexture(e)});break;case"skin":n=this.loadSkin(e);break;case"animation":n=this._invokeOne(function(s){return s.loadAnimation&&s.loadAnimation(e)});break;case"camera":n=this.loadCamera(e);break;default:if(n=this._invokeOne(function(s){return s!=this&&s.getDependency&&s.getDependency(t,e)}),!n)throw new Error("Unknown type: "+t);break}this.cache.add(r,n)}return n}getDependencies(t){let e=this.cache.get(t);if(!e){const r=this,n=this.json[t+(t==="mesh"?"es":"s")]||[];e=Promise.all(n.map(function(s,o){return r.getDependency(t,o)})),this.cache.add(t,e)}return e}loadBuffer(t){const e=this.json.buffers[t],r=this.fileLoader;if(e.type&&e.type!=="arraybuffer")throw new Error("THREE.GLTFLoader: "+e.type+" buffer type is not supported.");if(e.uri===void 0&&t===0)return Promise.resolve(this.extensions[w.KHR_BINARY_GLTF].body);const n=this.options;return new Promise(function(s,o){r.load(he.resolveURL(e.uri,n.path),s,void 0,function(){o(new Error('THREE.GLTFLoader: Failed to load buffer "'+e.uri+'".'))})})}loadBufferView(t){const e=this.json.bufferViews[t];return this.getDependency("buffer",e.buffer).then(function(r){const n=e.byteLength||0,s=e.byteOffset||0;return r.slice(s,s+n)})}loadAccessor(t){const e=this,r=this.json,n=this.json.accessors[t];if(n.bufferView===void 0&&n.sparse===void 0){const o=ke[n.type],a=oe[n.componentType],i=n.normalized===!0,c=new a(n.count*o);return Promise.resolve(new O(c,o,i))}const s=[];return n.bufferView!==void 0?s.push(this.getDependency("bufferView",n.bufferView)):s.push(null),n.sparse!==void 0&&(s.push(this.getDependency("bufferView",n.sparse.indices.bufferView)),s.push(this.getDependency("bufferView",n.sparse.values.bufferView))),Promise.all(s).then(function(o){const a=o[0],i=ke[n.type],c=oe[n.componentType],u=c.BYTES_PER_ELEMENT,l=u*i,h=n.byteOffset||0,f=n.bufferView!==void 0?r.bufferViews[n.bufferView].byteStride:void 0,m=n.normalized===!0;let x,p;if(f&&f!==l){const g=Math.floor(h/f),y="InterleavedBuffer:"+n.bufferView+":"+n.componentType+":"+g+":"+n.count;let R=e.cache.get(y);R||(x=new c(a,g*f,n.count*f/u),R=new on(x,f/u),e.cache.add(y,R)),p=new yn(R,i,h%f/u,m)}else a===null?x=new c(n.count*i):x=new c(a,h,n.count*i),p=new O(x,i,m);if(n.sparse!==void 0){const g=ke.SCALAR,y=oe[n.sparse.indices.componentType],R=n.sparse.indices.byteOffset||0,v=n.sparse.values.byteOffset||0,S=new y(o[1],R,n.sparse.count*g),M=new c(o[2],v,n.sparse.count*i);a!==null&&(p=new O(p.array.slice(),p.itemSize,p.normalized)),p.normalized=!1;for(let b=0,I=S.length;b<I;b++){const N=S[b];if(p.setX(N,M[b*i]),i>=2&&p.setY(N,M[b*i+1]),i>=3&&p.setZ(N,M[b*i+2]),i>=4&&p.setW(N,M[b*i+3]),i>=5)throw new Error("THREE.GLTFLoader: Unsupported itemSize in sparse BufferAttribute.")}p.normalized=m}return p})}loadTexture(t){const e=this.json,r=this.options,s=e.textures[t].source,o=e.images[s];let a=this.textureLoader;if(o.uri){const i=r.manager.getHandler(o.uri);i!==null&&(a=i)}return this.loadTextureImage(t,s,a)}loadTextureImage(t,e,r){const n=this,s=this.json,o=s.textures[t],a=s.images[e],i=(a.uri||a.bufferView)+":"+o.sampler;if(this.textureCache[i])return this.textureCache[i];const c=this.loadImageSource(e,r).then(function(u){u.flipY=!1,u.name=o.name||a.name||"",u.name===""&&typeof a.uri=="string"&&a.uri.startsWith("data:image/")===!1&&(u.name=a.uri);const h=(s.samplers||{})[o.sampler]||{};return u.magFilter=Tt[h.magFilter]||Ve,u.minFilter=Tt[h.minFilter]||Lt,u.wrapS=wt[h.wrapS]||Ke,u.wrapT=wt[h.wrapT]||Ke,u.generateMipmaps=!u.isCompressedTexture&&u.minFilter!==It&&u.minFilter!==Ve,n.associations.set(u,{textures:t}),u}).catch(function(){return null});return this.textureCache[i]=c,c}loadImageSource(t,e){const r=this,n=this.json,s=this.options;if(this.sourceCache[t]!==void 0)return this.sourceCache[t].then(l=>l.clone());const o=n.images[t],a=self.URL||self.webkitURL;let i=o.uri||"",c=!1;if(o.bufferView!==void 0)i=r.getDependency("bufferView",o.bufferView).then(function(l){c=!0;const h=new Blob([l],{type:o.mimeType});return i=a.createObjectURL(h),i});else if(o.uri===void 0)throw new Error("THREE.GLTFLoader: Image "+t+" is missing URI and bufferView");const u=Promise.resolve(i).then(function(l){return new Promise(function(h,f){let m=h;e.isImageBitmapLoader===!0&&(m=function(x){const p=new dt(x);p.needsUpdate=!0,h(p)}),e.load(he.resolveURL(l,s.path),m,void 0,f)})}).then(function(l){return c===!0&&a.revokeObjectURL(i),K(l,o),l.userData.mimeType=o.mimeType||ms(o.uri),l}).catch(function(l){throw console.error("THREE.GLTFLoader: Couldn't load texture",i),l});return this.sourceCache[t]=u,u}assignTexture(t,e,r,n){const s=this;return this.getDependency("texture",r.index).then(function(o){if(!o)return null;if(r.texCoord!==void 0&&r.texCoord>0&&(o=o.clone(),o.channel=r.texCoord),s.extensions[w.KHR_TEXTURE_TRANSFORM]){const a=r.extensions!==void 0?r.extensions[w.KHR_TEXTURE_TRANSFORM]:void 0;if(a){const i=s.associations.get(o);o=s.extensions[w.KHR_TEXTURE_TRANSFORM].extendTexture(o,a),s.associations.set(o,i)}}return n!==void 0&&(o.colorSpace=n),t[e]=o,o})}assignFinalMaterial(t){const e=t.geometry;let r=t.material;const n=e.attributes.tangent===void 0,s=e.attributes.color!==void 0,o=e.attributes.normal===void 0;if(t.isPoints){const a="PointsMaterial:"+r.uuid;let i=this.cache.get(a);i||(i=new dn,De.prototype.copy.call(i,r),i.color.copy(r.color),i.map=r.map,i.sizeAttenuation=!1,this.cache.add(a,i)),r=i}else if(t.isLine){const a="LineBasicMaterial:"+r.uuid;let i=this.cache.get(a);i||(i=new hn,De.prototype.copy.call(i,r),i.color.copy(r.color),i.map=r.map,this.cache.add(a,i)),r=i}if(n||s||o){let a="ClonedMaterial:"+r.uuid+":";n&&(a+="derivative-tangents:"),s&&(a+="vertex-colors:"),o&&(a+="flat-shading:");let i=this.cache.get(a);i||(i=r.clone(),s&&(i.vertexColors=!0),o&&(i.flatShading=!0),n&&(i.normalScale&&(i.normalScale.y*=-1),i.clearcoatNormalScale&&(i.clearcoatNormalScale.y*=-1)),this.cache.add(a,i),this.associations.set(i,this.associations.get(r))),r=i}t.material=r}getMaterialType(){return Nt}loadMaterial(t){const e=this,r=this.json,n=this.extensions,s=r.materials[t];let o;const a={},i=s.extensions||{},c=[];if(i[w.KHR_MATERIALS_UNLIT]){const l=n[w.KHR_MATERIALS_UNLIT];o=l.getMaterialType(),c.push(l.extendParams(a,s,e))}else{const l=s.pbrMetallicRoughness||{};if(a.color=new F(1,1,1),a.opacity=1,Array.isArray(l.baseColorFactor)){const h=l.baseColorFactor;a.color.setRGB(h[0],h[1],h[2],W),a.opacity=h[3]}l.baseColorTexture!==void 0&&c.push(e.assignTexture(a,"map",l.baseColorTexture,J)),a.metalness=l.metallicFactor!==void 0?l.metallicFactor:1,a.roughness=l.roughnessFactor!==void 0?l.roughnessFactor:1,l.metallicRoughnessTexture!==void 0&&(c.push(e.assignTexture(a,"metalnessMap",l.metallicRoughnessTexture)),c.push(e.assignTexture(a,"roughnessMap",l.metallicRoughnessTexture))),o=this._invokeOne(function(h){return h.getMaterialType&&h.getMaterialType(t)}),c.push(Promise.all(this._invokeAll(function(h){return h.extendMaterialParams&&h.extendMaterialParams(t,a)})))}s.doubleSided===!0&&(a.side=Ct);const u=s.alphaMode||He.OPAQUE;if(u===He.BLEND?(a.transparent=!0,a.depthWrite=!1):(a.transparent=!1,u===He.MASK&&(a.alphaTest=s.alphaCutoff!==void 0?s.alphaCutoff:.5)),s.normalTexture!==void 0&&o!==se&&(c.push(e.assignTexture(a,"normalMap",s.normalTexture)),a.normalScale=new C(1,1),s.normalTexture.scale!==void 0)){const l=s.normalTexture.scale;a.normalScale.set(l,l)}if(s.occlusionTexture!==void 0&&o!==se&&(c.push(e.assignTexture(a,"aoMap",s.occlusionTexture)),s.occlusionTexture.strength!==void 0&&(a.aoMapIntensity=s.occlusionTexture.strength)),s.emissiveFactor!==void 0&&o!==se){const l=s.emissiveFactor;a.emissive=new F().setRGB(l[0],l[1],l[2],W)}return s.emissiveTexture!==void 0&&o!==se&&c.push(e.assignTexture(a,"emissiveMap",s.emissiveTexture,J)),Promise.all(c).then(function(){const l=new o(a);return s.name&&(l.name=s.name),K(l,s),e.associations.set(l,{materials:t}),s.extensions&&Q(n,l,s),l})}createUniqueName(t){const e=fn.sanitizeNodeName(t||"");return e in this.nodeNamesUsed?e+"_"+ ++this.nodeNamesUsed[e]:(this.nodeNamesUsed[e]=0,e)}loadGeometries(t){const e=this,r=this.extensions,n=this.primitiveCache;function s(a){return r[w.KHR_DRACO_MESH_COMPRESSION].decodePrimitive(a,e).then(function(i){return xt(i,a,e)})}const o=[];for(let a=0,i=t.length;a<i;a++){const c=t[a],u=ps(c),l=n[u];if(l)o.push(l.promise);else{let h;c.extensions&&c.extensions[w.KHR_DRACO_MESH_COMPRESSION]?h=s(c):h=xt(new fe,c,e),n[u]={primitive:c,promise:h},o.push(h)}}return Promise.all(o)}loadMesh(t){const e=this,r=this.json,n=this.extensions,s=r.meshes[t],o=s.primitives,a=[];for(let i=0,c=o.length;i<c;i++){const u=o[i].material===void 0?ds(this.cache):this.getDependency("material",o[i].material);a.push(u)}return a.push(e.loadGeometries(o)),Promise.all(a).then(function(i){const c=i.slice(0,i.length-1),u=i[i.length-1],l=[];for(let f=0,m=u.length;f<m;f++){const x=u[f],p=o[f];let g;const y=c[f];if(p.mode===k.TRIANGLES||p.mode===k.TRIANGLE_STRIP||p.mode===k.TRIANGLE_FAN||p.mode===void 0)g=s.isSkinnedMesh===!0?new pn(x,y):new be(x,y),g.isSkinnedMesh===!0&&g.normalizeSkinWeights(),p.mode===k.TRIANGLE_STRIP?g.geometry=gt(g.geometry,Rt):p.mode===k.TRIANGLE_FAN&&(g.geometry=gt(g.geometry,ze));else if(p.mode===k.LINES)g=new Ot(x,y);else if(p.mode===k.LINE_STRIP)g=new mn(x,y);else if(p.mode===k.LINE_LOOP)g=new gn(x,y);else if(p.mode===k.POINTS)g=new We(x,y);else throw new Error("THREE.GLTFLoader: Primitive mode unsupported: "+p.mode);Object.keys(g.geometry.morphAttributes).length>0&&fs(g,s),g.name=e.createUniqueName(s.name||"mesh_"+t),K(g,s),p.extensions&&Q(n,g,p),e.assignFinalMaterial(g),l.push(g)}for(let f=0,m=l.length;f<m;f++)e.associations.set(l[f],{meshes:t,primitives:f});if(l.length===1)return s.extensions&&Q(n,l[0],s),l[0];const h=new Ee;s.extensions&&Q(n,h,s),e.associations.set(h,{meshes:t});for(let f=0,m=l.length;f<m;f++)h.add(l[f]);return h})}loadCamera(t){let e;const r=this.json.cameras[t],n=r[r.type];if(!n){console.warn("THREE.GLTFLoader: Missing camera parameters.");return}return r.type==="perspective"?e=new Ft(P.radToDeg(n.yfov),n.aspectRatio||1,n.znear||1,n.zfar||2e6):r.type==="orthographic"&&(e=new An(-n.xmag,n.xmag,n.ymag,-n.ymag,n.znear,n.zfar)),r.name&&(e.name=this.createUniqueName(r.name)),K(e,r),Promise.resolve(e)}loadSkin(t){const e=this.json.skins[t],r=[];for(let n=0,s=e.joints.length;n<s;n++)r.push(this._loadNodeShallow(e.joints[n]));return e.inverseBindMatrices!==void 0?r.push(this.getDependency("accessor",e.inverseBindMatrices)):r.push(null),Promise.all(r).then(function(n){const s=n.pop(),o=n,a=[],i=[];for(let c=0,u=o.length;c<u;c++){const l=o[c];if(l){a.push(l);const h=new Re;s!==null&&h.fromArray(s.array,c*16),i.push(h)}else console.warn('THREE.GLTFLoader: Joint "%s" could not be found.',e.joints[c])}return new Tn(a,i)})}loadAnimation(t){const e=this.json,r=this,n=e.animations[t],s=n.name?n.name:"animation_"+t,o=[],a=[],i=[],c=[],u=[];for(let l=0,h=n.channels.length;l<h;l++){const f=n.channels[l],m=n.samplers[f.sampler],x=f.target,p=x.node,g=n.parameters!==void 0?n.parameters[m.input]:m.input,y=n.parameters!==void 0?n.parameters[m.output]:m.output;x.node!==void 0&&(o.push(this.getDependency("node",p)),a.push(this.getDependency("accessor",g)),i.push(this.getDependency("accessor",y)),c.push(m),u.push(x))}return Promise.all([Promise.all(o),Promise.all(a),Promise.all(i),Promise.all(c),Promise.all(u)]).then(function(l){const h=l[0],f=l[1],m=l[2],x=l[3],p=l[4],g=[];for(let y=0,R=h.length;y<R;y++){const v=h[y],S=f[y],M=m[y],b=x[y],I=p[y];if(v===void 0)continue;v.updateMatrix&&v.updateMatrix();const N=r._createAnimationTracks(v,S,M,b,I);if(N)for(let B=0;B<N.length;B++)g.push(N[B])}return new wn(s,void 0,g)})}createNodeMesh(t){const e=this.json,r=this,n=e.nodes[t];return n.mesh===void 0?null:r.getDependency("mesh",n.mesh).then(function(s){const o=r._getNodeRef(r.meshCache,n.mesh,s);return n.weights!==void 0&&o.traverse(function(a){if(a.isMesh)for(let i=0,c=n.weights.length;i<c;i++)a.morphTargetInfluences[i]=n.weights[i]}),o})}loadNode(t){const e=this.json,r=this,n=e.nodes[t],s=r._loadNodeShallow(t),o=[],a=n.children||[];for(let c=0,u=a.length;c<u;c++)o.push(r.getDependency("node",a[c]));const i=n.skin===void 0?Promise.resolve(null):r.getDependency("skin",n.skin);return Promise.all([s,Promise.all(o),i]).then(function(c){const u=c[0],l=c[1],h=c[2];h!==null&&u.traverse(function(f){f.isSkinnedMesh&&f.bind(h,gs)});for(let f=0,m=l.length;f<m;f++)u.add(l[f]);return u})}_loadNodeShallow(t){const e=this.json,r=this.extensions,n=this;if(this.nodeCache[t]!==void 0)return this.nodeCache[t];const s=e.nodes[t],o=s.name?n.createUniqueName(s.name):"",a=[],i=n._invokeOne(function(c){return c.createNodeMesh&&c.createNodeMesh(t)});return i&&a.push(i),s.camera!==void 0&&a.push(n.getDependency("camera",s.camera).then(function(c){return n._getNodeRef(n.cameraCache,s.camera,c)})),n._invokeAll(function(c){return c.createNodeAttachment&&c.createNodeAttachment(t)}).forEach(function(c){a.push(c)}),this.nodeCache[t]=Promise.all(a).then(function(c){let u;if(s.isBone===!0?u=new xn:c.length>1?u=new Ee:c.length===1?u=c[0]:u=new Mt,u!==c[0])for(let l=0,h=c.length;l<h;l++)u.add(c[l]);if(s.name&&(u.userData.name=s.name,u.name=o),K(u,s),s.extensions&&Q(r,u,s),s.matrix!==void 0){const l=new Re;l.fromArray(s.matrix),u.applyMatrix4(l)}else s.translation!==void 0&&u.position.fromArray(s.translation),s.rotation!==void 0&&u.quaternion.fromArray(s.rotation),s.scale!==void 0&&u.scale.fromArray(s.scale);return n.associations.has(u)||n.associations.set(u,{}),n.associations.get(u).nodes=t,u}),this.nodeCache[t]}loadScene(t){const e=this.extensions,r=this.json.scenes[t],n=this,s=new Ee;r.name&&(s.name=n.createUniqueName(r.name)),K(s,r),r.extensions&&Q(e,s,r);const o=r.nodes||[],a=[];for(let i=0,c=o.length;i<c;i++)a.push(n.getDependency("node",o[i]));return Promise.all(a).then(function(i){for(let u=0,l=i.length;u<l;u++)s.add(i[u]);const c=u=>{const l=new Map;for(const[h,f]of n.associations)(h instanceof De||h instanceof dt)&&l.set(h,f);return u.traverse(h=>{const f=n.associations.get(h);f!=null&&l.set(h,f)}),l};return n.associations=c(s),s})}_createAnimationTracks(t,e,r,n,s){const o=[],a=t.name?t.name:t.uuid,i=[];q[s.path]===q.weights?t.traverse(function(h){h.morphTargetInfluences&&i.push(h.name?h.name:h.uuid)}):i.push(a);let c;switch(q[s.path]){case q.weights:c=ft;break;case q.rotation:c=pt;break;case q.position:case q.scale:c=ht;break;default:switch(r.itemSize){case 1:c=ft;break;case 2:case 3:default:c=ht;break}break}const u=n.interpolation!==void 0?us[n.interpolation]:Pt,l=this._getArrayFromAccessor(r);for(let h=0,f=i.length;h<f;h++){const m=new c(i[h]+"."+q[s.path],e.array,l,u);n.interpolation==="CUBICSPLINE"&&this._createCubicSplineTrackInterpolant(m),o.push(m)}return o}_getArrayFromAccessor(t){let e=t.array;if(t.normalized){const r=Ye(e.constructor),n=new Float32Array(e.length);for(let s=0,o=e.length;s<o;s++)n[s]=e[s]*r;e=n}return e}_createCubicSplineTrackInterpolant(t){t.createInterpolant=function(r){const n=this instanceof pt?ls:Ht;return new n(this.times,this.values,this.getValueSize()/3,r)},t.createInterpolant.isInterpolantFactoryMethodGLTFCubicSpline=!0}}function Ts(d,t,e){const r=t.attributes,n=new Dt;if(r.POSITION!==void 0){const a=e.json.accessors[r.POSITION],i=a.min,c=a.max;if(i!==void 0&&c!==void 0){if(n.set(new T(i[0],i[1],i[2]),new T(c[0],c[1],c[2])),a.normalized){const u=Ye(oe[a.componentType]);n.min.multiplyScalar(u),n.max.multiplyScalar(u)}}else{console.warn("THREE.GLTFLoader: Missing min/max properties for accessor POSITION.");return}}else return;const s=t.targets;if(s!==void 0){const a=new T,i=new T;for(let c=0,u=s.length;c<u;c++){const l=s[c];if(l.POSITION!==void 0){const h=e.json.accessors[l.POSITION],f=h.min,m=h.max;if(f!==void 0&&m!==void 0){if(i.setX(Math.max(Math.abs(f[0]),Math.abs(m[0]))),i.setY(Math.max(Math.abs(f[1]),Math.abs(m[1]))),i.setZ(Math.max(Math.abs(f[2]),Math.abs(m[2]))),h.normalized){const x=Ye(oe[h.componentType]);i.multiplyScalar(x)}a.max(i)}else console.warn("THREE.GLTFLoader: Missing min/max properties for accessor POSITION.")}}n.expandByVector(a)}d.boundingBox=n;const o=new Xe;n.getCenter(o.center),o.radius=n.min.distanceTo(n.max)/2,d.boundingSphere=o}function xt(d,t,e){const r=t.attributes,n=[];function s(o,a){return e.getDependency("accessor",o).then(function(i){d.setAttribute(a,i)})}for(const o in r){const a=qe[o]||o.toLowerCase();a in d.attributes||n.push(s(r[o],a))}if(t.indices!==void 0&&!d.index){const o=e.getDependency("accessor",t.indices).then(function(a){d.setIndex(a)});n.push(o)}return mt.workingColorSpace!==W&&"COLOR_0"in r&&console.warn(`THREE.GLTFLoader: Converting vertex colors from "srgb-linear" to "${mt.workingColorSpace}" not supported.`),K(d,t),Ts(d,t,e),Promise.all(n).then(function(){return t.targets!==void 0?hs(d,t.targets,e):d})}function Bt(d,t){const{count:e,size:r=10}=t,n=d.index?d.toNonIndexed():d.clone();n.computeVertexNormals();const s=n.getAttribute("position"),o=n.getAttribute("normal"),a=Math.floor(s.count/3),i=new Float64Array(a),c=new T,u=new T,l=new T,h=new T,f=new T,m=new T;let x=0;for(let E=0;E<a;E+=1)c.fromBufferAttribute(s,E*3),u.fromBufferAttribute(s,E*3+1),l.fromBufferAttribute(s,E*3+2),h.subVectors(u,c),f.subVectors(l,c),x+=m.crossVectors(h,f).length()*.5,i[E]=x;const p=new Float32Array(e*3),g=new Float32Array(e*3),y=new T,R=new T,v=new T,S=new T,M=new T,b=new Dt;for(let E=0;E<e;E+=1){const pe=Math.random()*x;let D=0,Z=a-1;for(;D<Z;){const U=D+Z>>1;(i[U]??0)<pe?D=U+1:Z=U}c.fromBufferAttribute(s,D*3),u.fromBufferAttribute(s,D*3+1),l.fromBufferAttribute(s,D*3+2),y.fromBufferAttribute(o,D*3),R.fromBufferAttribute(o,D*3+1),v.fromBufferAttribute(o,D*3+2);const re=Math.sqrt(Math.random()),G=Math.random(),$=1-re,ee=re*(1-G),Y=re*G;S.set(0,0,0).addScaledVector(c,$).addScaledVector(u,ee).addScaledVector(l,Y),M.set(0,0,0).addScaledVector(y,$).addScaledVector(R,ee).addScaledVector(v,Y).normalize(),p[E*3]=S.x,p[E*3+1]=S.y,p[E*3+2]=S.z,g[E*3]=M.x,g[E*3+1]=M.y,g[E*3+2]=M.z,b.expandByPoint(S)}n.dispose();const I=b.getCenter(new T),N=b.getSize(new T),B=r/Math.max(N.x,N.y,N.z,1e-6);for(let E=0;E<e;E+=1)p[E*3]=(p[E*3]-I.x)*B,p[E*3+1]=(p[E*3+1]-I.y)*B,p[E*3+2]=(p[E*3+2]-I.z)*B;return{positions:p,normals:g,count:e}}async function ws(d,t){try{const r=await new Hn().loadAsync(d),n=[];if(r.scene.updateMatrixWorld(!0),r.scene.traverse(a=>{if(!(a instanceof be))return;const i=a.geometry.clone();i.applyMatrix4(a.matrixWorld);for(const c of Object.keys(i.attributes))c!=="position"&&c!=="normal"&&i.deleteAttribute(c);n.push(i.index?i.toNonIndexed():i)}),n.length===0)return null;const s=xs(n),o=Bt(s,t);s.dispose();for(const a of n)a.dispose();return o}catch{return null}}function xs(d){let t=0;for(const o of d)t+=o.getAttribute("position").count;const e=new Float32Array(t*3),r=new Float32Array(t*3);let n=0;for(const o of d){const a=o.getAttribute("position"),i=o.getAttribute("normal");for(let c=0;c<a.count;c+=1)e[(n+c)*3]=a.getX(c),e[(n+c)*3+1]=a.getY(c),e[(n+c)*3+2]=a.getZ(c),i&&(r[(n+c)*3]=i.getX(c),r[(n+c)*3+1]=i.getY(c),r[(n+c)*3+2]=i.getZ(c));n+=a.count}const s=new fe;return s.setAttribute("position",new O(e,3)),s.setAttribute("normal",new O(r,3)),s}function vs(){const d=[new C(0,.62),new C(.18,.58),new C(.3,.44),new C(.34,.4),new C(1.45,-.34),new C(1.6,-.4),new C(1.74,-.22),new C(1.88,-.38),new C(2,-.42),new C(2.06,-.42)],t=new bn(d,96);return t.rotateX(-Math.PI/2),t.computeVertexNormals(),t}const Gt=1180/720,de=6,Ge=[8,11,10,7.2,8.4,5.8],Ue=[8,11,4.7,5.9,7.2,6.5],ys=.78,Es=.92,Rs=.17,bs=-.05;function vt(d){return Ge[Math.max(0,Math.min(Ge.length-1,d))]??Ge[0]}function yt(d){const t=Math.max(0,Math.min(Ue.length-1,d));return Ue[t]??Ue[0]}const Ss=.8,Ms=.34,_s=2.4,je=120;function Ls(){const d=document.querySelector('meta[name="gnarl-model"]')?.getAttribute("content")?.trim();return d||null}const Et=`
  vec3 hash3(vec3 p) {
    p = vec3(
      dot(p, vec3(127.1, 311.7, 74.7)),
      dot(p, vec3(269.5, 183.3, 246.1)),
      dot(p, vec3(113.5, 271.9, 124.6))
    );
    return fract(sin(p) * 43758.5453123);
  }

  float valueNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);

    // Smoothstep rather than the raw fraction: linear interpolation leaves
    // the cell boundaries visible as a lattice, which on a dissolve reads as
    // a grid of square holes.
    vec3 u = f * f * (3.0 - 2.0 * f);

    float n000 = hash3(i + vec3(0.0, 0.0, 0.0)).x;
    float n100 = hash3(i + vec3(1.0, 0.0, 0.0)).x;
    float n010 = hash3(i + vec3(0.0, 1.0, 0.0)).x;
    float n110 = hash3(i + vec3(1.0, 1.0, 0.0)).x;
    float n001 = hash3(i + vec3(0.0, 0.0, 1.0)).x;
    float n101 = hash3(i + vec3(1.0, 0.0, 1.0)).x;
    float n011 = hash3(i + vec3(0.0, 1.0, 1.0)).x;
    float n111 = hash3(i + vec3(1.0, 1.0, 1.0)).x;

    return mix(
      mix(mix(n000, n100, u.x), mix(n010, n110, u.x), u.y),
      mix(mix(n001, n101, u.x), mix(n011, n111, u.x), u.y),
      u.z
    );
  }

  /*  Two octaves. One is too smooth - the holes open as a few big round
      blobs, which reads as melting rather than burning. */
  float dissolveField(vec3 p) {
    return valueNoise(p * 1.7) * 0.68 + valueNoise(p * 4.3) * 0.32;
  }
`,Is=`
  const float TAU = 6.28318530718;

  /*  ORBITS. Great circles at scattered orientations: a woven ball of light
      with a bright core, which is what the page opens on. */
  vec3 formOrbits(float c, float t, vec3 s) {
    float a = t * TAU;
    vec3 ring = vec3(cos(a), sin(a), 0.0) * (5.4 + s.x * 2.6);

    float tilt = c * 6.2831853 + s.y * 0.7;
    float spin = c * 7.0 + s.z * 1.3;

    vec3 p = vec3(
      ring.x,
      ring.y * cos(tilt) - ring.z * sin(tilt),
      ring.y * sin(tilt) + ring.z * cos(tilt)
    );

    return vec3(
      p.x * cos(spin) + p.z * sin(spin),
      p.y,
      -p.x * sin(spin) + p.z * cos(spin)
    );
  }

  /*  THE OSCILLATOR'S OWN SURFACE, drawn as its rows - which is what a
      wavetable is, and how the plugin's own display draws it. */
  vec3 formWave(float c, float t, vec3 s) {
    float x = (t - 0.5) * 22.0;
    float z = (c - 0.5) * 14.0;

    float pos = 0.5 + 0.5 * sin(uTime * 0.22);
    float y =
        sin(x * 0.55 + uTime * 0.6) * 1.7
      + sin(x * 1.30 - uTime * 0.35) * 0.85 * pos
      + sin(x * 2.10 + uTime * 0.80) * 0.40 * pos * pos;

    y *= 1.0 - c * 0.45;

    return vec3(x, y, z);
  }

  /*  MODULATION. A double helix: a mod slot is a source and a destination
      travelling together, so the strands are paired. */
  vec3 formHelix(float c, float t, vec3 s) {
    float strand = step(0.5, fract(c * 8.0));
    float a = t * TAU * 2.2 + strand * 3.14159265 + c * 0.7;
    float r = 4.0 + s.x * 0.7;

    return vec3(cos(a) * r, (t - 0.5) * 20.0, sin(a) * r);
  }

  /*  THE DRIVER, as concentric rings - the same cone the solid mesh is, so
      the solid can cross-fade IN over lines that already have its shape.
      That is what makes the fourth transition a morph rather than a cut. */
  vec3 formDriver(float c, float t, vec3 s) {
    float a = t * TAU;
    float r = 0.35 + c * 5.4;

    // The lathe's profile, roughly: dust cap forward, cone falling away,
    // surround rolling back up at the rim.
    float depth = 1.9 - r * 0.62 + smoothstep(4.6, 5.4, r) * 0.9;

    return vec3(cos(a) * r, sin(a) * r, depth);
  }

  /*  THE INSTRUMENT. Its own rectangle, ruled as a grid, at its real aspect
      ratio - the screenshot then fades up inside it. */
  vec3 formPanel(float c, float t, vec3 s) {
    float W = 13.0;
    float H = ${(13/Gt).toFixed(4)};

    float across = step(0.5, fract(c * 2.0));
    float lane = floor(c * 30.0) / 29.0;

    vec2 p = across > 0.5
      ? vec2((t - 0.5) * W, (lane - 0.5) * H)
      : vec2((lane - 0.5) * W, (t - 0.5) * H);

    return vec3(p, (s.z - 0.5) * 0.25);
  }

  /*  THE CRYSTAL. What the dissolve's dust settles back into, and the
      section where the thing stops being a process and becomes a product -
      so it is the one formation here with flat facets and hard edges rather
      than swept curves. A polygon, not a circle: the exact regular-polygon
      radius, so the rings are octagons and read as cut faces.

      It sits between the dissolve and the interface for that reason. Dust,
      then something faceted, then the instrument. */
  vec3 formCrystal(float c, float t, vec3 s) {
    const float FACETS = 8.0;
    const float SEG = TAU / FACETS;

    // Exact regular polygon of FACETS sides, radius 1 at its vertices.
    float a = t * TAU;
    float poly = cos(SEG * 0.5) / cos(mod(a, SEG) - SEG * 0.5);

    float ring = step(0.5, fract(c * 2.0));
    float lane = floor(c * 14.0) / 13.0;

    /*  A gem's profile: a point at the bottom, the widest band a quarter of
        the way up, and a flat table on top. The girdle sits low because a
        stone with its widest point in the middle reads as a ball. */
    float h = (lane - 0.5) * 2.0;
    float girdle = 0.25;
    float radius = h < girdle
      ? (h + 1.0) / (1.0 + girdle)
      : 1.0 - (h - girdle) / (1.0 - girdle) * 0.55;

    radius = max(radius, 0.04) * 7.2;

    if (ring > 0.5) {
      // A horizontal facet edge.
      return vec3(cos(a) * radius * poly, h * 6.0, sin(a) * radius * poly);
    }

    /*  A vertical facet: girdle vertex up to the table, or down to the
        point. t runs the whole edge, so the stroke is one straight line
        rather than something that has to be chased around the shape.

        No backticks anywhere in this block: FORMATIONS is a template
        literal, so one inside a GLSL comment ends the string and the rest
        of the shader is parsed as TypeScript. */
    float vertex = floor(c * FACETS * 2.0);
    float va = (vertex / FACETS) * SEG * FACETS / FACETS;
    va = floor(mod(vertex, FACETS)) * SEG;

    float upper = step(0.5, fract(vertex / 2.0));
    float wide = 7.2;

    vec3 girdlePoint = vec3(cos(va) * wide, girdle * 6.0, sin(va) * wide);
    vec3 tip = upper > 0.5
      ? vec3(cos(va) * wide * 0.45, 6.0, sin(va) * wide * 0.45)
      : vec3(0.0, -6.0, 0.0);

    return mix(girdlePoint, tip, t);
  }

  vec3 formation(int id, float c, float t, vec3 s) {
    if (id <= 0) return formOrbits(c, t, s);
    if (id == 1) return formWave(c, t, s);
    if (id == 2) return formHelix(c, t, s);
    if (id == 3) return formDriver(c, t, s);
    if (id == 4) return formCrystal(c, t, s);
    return formPanel(c, t, s);
  }
`;async function Ps(d){const t=window.matchMedia("(prefers-reduced-motion: reduce)").matches;let e=window.innerWidth<860;const r=new Sn({canvas:d,antialias:!e,powerPreference:"high-performance"});r.setPixelRatio(Math.min(window.devicePixelRatio,e?1.25:2)),r.toneMapping=Mn,r.toneMappingExposure=1;const n=new _n;n.background=new F(197386);const s=new Ft(52,1,.1,900),o=[],a=new Ln([new T(0,0,0),new T(22,8,-60),new T(-14,-6,-130),new T(18,10,-205),new T(-8,2,-280),new T(6,-4,-350)],!1,"catmullrom",.5),i=e?4500:12e3,c=new fe,u=new Float32Array(i*3),l=new Float32Array(i*2),h=new T;for(let A=0;A<i;A+=1){a.getPointAt(Math.random(),h);const _=45+Math.random()*130,H=Math.random()*Math.PI*2,L=Math.acos(1-2*Math.random());u[A*3]=h.x+_*Math.sin(L)*Math.cos(H),u[A*3+1]=h.y+_*Math.cos(L),u[A*3+2]=h.z+_*Math.sin(L)*Math.sin(H),l[A*2]=Math.random(),l[A*2+1]=Math.random()}c.setAttribute("position",new O(u,3)),c.setAttribute("aSeed",new O(l,2)),o.push(c);const f={uTime:{value:0},uPixelRatio:{value:r.getPixelRatio()}},m=new ve({uniforms:f,transparent:!0,depthWrite:!1,blending:ye,vertexShader:`
      precision highp float;

      attribute vec2 aSeed;
      uniform float uTime;
      uniform float uPixelRatio;

      varying float vBright;
      varying float vWarm;

      void main() {
        vec4 mv = modelViewMatrix * vec4(position, 1.0);

        /*  Unequal, and steeply so. A field of equally bright dots reads as
            noise; a few bright among many faint reads as a sky. */
        float b = aSeed.x * aSeed.x * aSeed.x * aSeed.x;
        b *= 0.65 + 0.35 * sin(uTime * 0.6 + aSeed.y * 90.0);

        vBright = b;
        vWarm = aSeed.y;

        // Attenuated with distance, or the far half of a volume this deep
        // is the same size as the near half and the depth disappears.
        gl_PointSize = (0.6 + b * 3.0) * uPixelRatio
                     * (90.0 / max(-mv.z, 1.0));

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
    `});o.push(m);const x=new We(c,m);x.frustumCulled=!1,n.add(x);const p=new Ee;n.add(p);const g=e?34:56,y=je*g*2,R=new fe,v=new Float32Array(y),S=new Float32Array(y),M=new Float32Array(y*3);let b=0;for(let A=0;A<je;A+=1){const _=A/(je-1),H=Math.random(),L=Math.random(),j=Math.random();for(let z=0;z<g;z+=1)for(const xe of[z/g,(z+1)/g])v[b]=_,S[b]=xe,M[b*3]=H,M[b*3+1]=L,M[b*3+2]=j,b+=1}R.setAttribute("position",new O(new Float32Array(y*3),3)),R.setAttribute("aCurve",new O(v,1)),R.setAttribute("aT",new O(S,1)),R.setAttribute("aSeed",new O(M,3)),R.boundingSphere=new Xe(new T,40),o.push(R);const I={uForm:{value:0},uTime:{value:0},uFade:{value:1},uAssemble:{value:0},uAccent:{value:new F(8251135)},uAccent2:{value:new F(12160255)}},N=new ve({uniforms:I,transparent:!0,depthWrite:!1,blending:ye,vertexShader:`
      precision highp float;

      attribute float aCurve;
      attribute float aT;
      attribute vec3  aSeed;

      uniform float uForm;
      uniform float uTime;
      uniform float uAssemble;

      varying float vGlow;
      varying float vDepth;
      varying float vBuild;

      ${Is}

      /*  WHERE EACH STROKE COMES IN FROM. A direction off the seed rather
          than a scatter of absolute positions, so every stroke flies in
          along its own line from its own side and the figure converges from
          all around instead of expanding out of a point - which is what
          makes it read as being BUILT rather than zoomed. */
      vec3 approach(vec3 s) {
        float theta = s.x * 6.2831853;
        float phi   = s.y * 3.14159265;

        return vec3(
          sin(phi) * cos(theta),
          cos(phi),
          sin(phi) * sin(theta)
        ) * (26.0 + s.z * 46.0);
      }

      void main() {
        float f = clamp(uForm, 0.0, ${(de-1).toFixed(1)});
        int   a = int(floor(f));
        int   b = int(min(floor(f) + 1.0, ${(de-1).toFixed(1)}));
        float t = fract(f);

        /*  Staggered by curve, and eased. Blending every vertex at once
            turns the figure into a smear for the middle of the transition;
            staggering means curves leave and arrive at different moments,
            so it reorganises itself rather than being squashed. */
        float stagger = aSeed.x * 0.35;
        float te = smoothstep(0.0, 1.0, clamp((t - stagger) / 0.65, 0.0, 1.0));

        vec3 p = mix(
          formation(a, aCurve, aT, aSeed),
          formation(b, aCurve, aT, aSeed),
          te
        );

        /*  THE BUILD. Each stroke has its own slice of the window, keyed off
            its seed, so they land over a period rather than all at once - a
            figure whose every stroke arrives on the same frame is a figure
            that pops into being, which is the thing this exists to avoid.
            The last strokes are still settling as the first are already
            holding, and that overlap is the whole effect. */
        float bDelay = aSeed.y * 0.42 + aSeed.z * 0.12;
        float bt = clamp((uAssemble - bDelay) / 0.46, 0.0, 1.0);

        // Cubic ease out: fast out of the dark, decelerating onto the mark.
        float be = 1.0 - pow(1.0 - bt, 3.0);

        vec3 from = p + approach(aSeed);

        // A little spin on the way in, unwinding to nothing as it lands.
        float swirl = (1.0 - be) * (aSeed.x - 0.5) * 2.4;
        float cs = cos(swirl);
        float sn = sin(swirl);
        from = vec3(from.x * cs - from.z * sn, from.y, from.x * sn + from.z * cs);

        p = mix(from, p, be);

        vec4 mv = modelViewMatrix * vec4(p, 1.0);

        vGlow  = 1.0 - abs(te * 2.0 - 1.0);
        vDepth = clamp((length(p) - 2.0) / 12.0, 0.0, 1.0);
        vBuild = be;

        gl_Position = projectionMatrix * mv;
      }
    `,fragmentShader:`
      precision highp float;

      uniform vec3  uAccent;
      uniform vec3  uAccent2;
      uniform float uFade;

      varying float vGlow;
      varying float vDepth;
      varying float vBuild;

      void main() {
        if (uFade <= 0.001) discard;

        // Cyan at the core, violet at the edges - the plugin's own two
        // accents, in the roles they hold there: cyan is what is live.
        vec3 colour = mix(uAccent, uAccent2, vDepth) + vGlow * 0.7;

        /*  A stroke still travelling is brighter and thinner than one that
            has landed, so the incoming ones read as live and the settled
            ones sit back. Without this the build is only movement; with it
            the figure looks like it is being drawn. */
        float arriving = 1.0 - vBuild;
        colour += arriving * 0.55;

        gl_FragColor = vec4(colour, (0.26 + vGlow * 0.34) * uFade * (0.35 + vBuild * 0.65));
      }
    `});o.push(N);const B=new Ot(R,N);B.frustumCulled=!1,p.add(B);const E=new In({map:Ns(),color:12577279,transparent:!0,blending:ye,depthWrite:!1});o.push(E);const pe=new Nn(E);p.add(pe);const D=e?22e3:55e3,Z=Ls(),re=Z?await ws(Z,{count:D,size:11}):null,G=vs();G.computeBoundingBox();const $=G.boundingBox.getSize(new T),ee=11/Math.max($.x,$.y,$.z);G.scale(ee,ee,ee),G.center(),o.push(G);const Y=re??Bt(G,{count:D,size:11}),U={uThreshold:{value:-.2},uEdge:{value:.07},uFade:{value:0},uBody:{value:new F(2761040)},uRim:{value:new F(8251135)},uHot:{value:new F(16751164)}},Qe=new ve({uniforms:U,side:Ct,transparent:!0,vertexShader:`
      precision highp float;

      varying vec3 vObject;
      varying vec3 vNormal;
      varying vec3 vView;

      void main() {
        // OBJECT space: in world space the noise is nailed to the room and
        // rotating the mesh makes the burn crawl across it like a
        // searchlight rather than like the thing itself decaying.
        vObject = position;
        vNormal = normalize(normalMatrix * normal);

        vec4 mv = modelViewMatrix * vec4(position, 1.0);
        vView = -mv.xyz;

        gl_Position = projectionMatrix * mv;
      }
    `,fragmentShader:`
      precision highp float;

      uniform float uThreshold;
      uniform float uEdge;
      uniform float uFade;
      uniform vec3  uBody;
      uniform vec3  uRim;
      uniform vec3  uHot;

      varying vec3 vObject;
      varying vec3 vNormal;
      varying vec3 vView;

      ${Et}

      void main() {
        if (uFade <= 0.001) discard;

        float n = dissolveField(vObject);
        if (n < uThreshold) discard;

        /*  The band of surface ABOUT to go. Its width is in field units, so
            it stays the same visual thickness however fast the threshold is
            moving - tied to the threshold's speed instead, a fast scroll
            would smear and a slow one would show no edge at all. */
        float edge = 1.0 - smoothstep(uThreshold, uThreshold + uEdge, n);

        /*  A rim light. There are no lights in this scene; this is the
            cheapest thing that gives a surface a form, and it costs one dot
            product. At any more than a third it becomes a halo and the form
            disappears into its own glow. */
        float facing = 1.0 - abs(dot(normalize(vNormal), normalize(vView)));
        vec3 colour = uBody + uRim * pow(clamp(facing, 0.0, 1.0), 2.2) * 0.32;

        colour = mix(colour, uHot, edge * 0.9);
        colour += vec3(1.0) * pow(edge, 6.0) * 1.1;

        gl_FragColor = vec4(colour, uFade);
      }
    `});o.push(Qe);const Se=new be(G,Qe);p.add(Se);const te=new fe;te.setAttribute("position",new O(Y.positions,3)),te.setAttribute("aNormal",new O(Y.normals,3));const Je=new Float32Array(Y.count*3);for(let A=0;A<Y.count*3;A+=1)Je[A]=Math.random();te.setAttribute("aSeed",new O(Je,3)),te.boundingSphere=new Xe(new T,60),o.push(te);const Me={uThreshold:U.uThreshold,uFade:U.uFade,uTime:{value:0},uPixelRatio:{value:r.getPixelRatio()},uHot:U.uHot,uCool:{value:new F(8251135)}},Ze=new ve({uniforms:Me,transparent:!0,depthWrite:!1,blending:ye,vertexShader:`
      precision highp float;

      attribute vec3 aNormal;
      attribute vec3 aSeed;

      uniform float uThreshold;
      uniform float uTime;
      uniform float uPixelRatio;

      varying float vAge;

      ${Et}

      void main() {
        /*  THE SAME FIELD AT THE SAME POINT the surface shader reads, so
            this mote lets go exactly when the surface beneath it opens. */
        float n = dissolveField(position);

        /*  The window and the threshold's end have to agree. The field tops
            out at 1, so the threshold must clear 1 by MORE than this window
            or the last motes released are still at full brightness when the
            page ends - and sixty thousand of those bunched together is a
            white wall, which is exactly what it was. */
        float age = clamp((uThreshold - n) / 0.35, 0.0, 1.0);
        vAge = age;

        vec3 p = position;

        if (age > 0.0) {
          // Off along the normal first - dust leaves a surface
          // perpendicular to it - then drifting and rising. The cube is what
          // makes the release feel like one: slow at the lip, quick once free.
          float travel = age * age * age * 9.0;

          p += aNormal * travel;
          p += vec3(
            sin(uTime * 0.7 + aSeed.x * 40.0),
            cos(uTime * 0.5 + aSeed.y * 40.0) + 1.3,
            sin(uTime * 0.6 + aSeed.z * 40.0)
          ) * travel * 0.45;
        }

        vec4 mv = modelViewMatrix * vec4(p, 1.0);

        gl_PointSize = (1.0 + aSeed.x * 1.6) * (1.0 - age * 0.7)
                     * uPixelRatio * (85.0 / max(-mv.z, 0.001));

        gl_Position = projectionMatrix * mv;
      }
    `,fragmentShader:`
      precision highp float;

      uniform vec3  uHot;
      uniform vec3  uCool;
      uniform float uFade;

      varying float vAge;

      void main() {
        // Motes not yet released must not draw, or the solid object wears a
        // haze of dust it has not shed.
        if (vAge <= 0.0 || uFade <= 0.001) discard;

        vec2 d = gl_PointCoord - 0.5;
        float r = length(d);
        if (r > 0.5) discard;

        float alpha = smoothstep(0.5, 0.0, r);

        // Hot at release, cooling to the instrument's cyan as it drifts.
        vec3 colour = mix(uHot, uCool, smoothstep(0.0, 0.45, vAge));

        /*  LOW. Fifty-five thousand additive sprites is a lot of light: at
            the alpha that looks right for ONE the cloud sums to flat white
            and the bloom turns the frame into a lamp. */
        gl_FragColor = vec4(colour, alpha * (1.0 - vAge) * 0.115 * uFade);
      }
    `});o.push(Ze);const _e=new We(te,Ze);_e.frustumCulled=!1,p.add(_e);const Le=new _t().load("./ui-osc.webp");Le.colorSpace=J,o.push(Le);const Ie=new se({map:Le,transparent:!0,opacity:0,toneMapped:!1,depthWrite:!1});o.push(Ie);const ie=new be(new Cn(13,13/Gt),Ie);o.push(ie.geometry),p.add(ie);const ne=new On(r);ne.addPass(new Fn(n,s));const $e=new Pn(new C(1,1),e?.6:.82,.66,.28);ne.addPass($e),ne.addPass(new Dn);const Ne=new C(.5,.5),me=new C(.5,.5);let ge=0,ae=0;const X=new T,et=new T,tt=new T,Ce=new T;let nt=!1,Oe=1;const Ae=new T,ce=new T,le=new T,Te=new T,st=new T;a.getPointAt(0,s.position),s.position.z+=26;let ot=1,rt=0,we=0;function Ut(){const A=d.clientWidth,_=d.clientHeight;e=window.innerWidth<860,r.setSize(A,_,!1),ne.setSize(A,_);const H=e?.5:1;$e.setSize(A*H,_*H),f.uPixelRatio.value=r.getPixelRatio(),Me.uPixelRatio.value=r.getPixelRatio(),s.aspect=A/_,s.fov=e?66:52,s.updateProjectionMatrix();const L=A!==rt,j=Math.abs(_-we);(L||j>we*.25||we===0)&&(rt=A,we=_,ot=A/Math.max(1,_))}return{setScroll(A){ge=A},setPointer(A,_){Ne.set(A,_)},isSettled(){return nt},setEntry(A){Oe=Math.max(0,Math.min(1,A))},setAssemble(A){I.uAssemble.value=Math.max(0,Math.min(1,A))},resize:Ut,render(A,_){I.uTime.value=A,f.uTime.value=A,Me.uTime.value=A;const H=Fe=>1-Math.exp(-Fe*Math.min(_,.1));t?(ae=ge,me.copy(Ne)):(ae+=(ge-ae)*H(4),me.lerp(Ne,H(2.5)));const L=Math.min(1,Math.max(0,ae));a.getPointAt(L,X),a.getPointAt(Math.max(0,L-.055),et),a.getPointAt(Math.min(1,L+.02),tt),p.position.copy(X),le.subVectors(et,tt).normalize().multiplyScalar(.86),le.y+=.21+(me.y-.5)*-.16,le.x+=(me.x-.5)*.24,le.normalize();const j=L*(de-1),z=Math.min(de-1,Math.floor(j)),xe=Math.min(de-1,z+1),jt=P.lerp(vt(z),vt(xe),j-z),zt=P.lerp(yt(z),yt(xe),j-z),Vt=e?Ms:Ss,it=Math.tan(s.fov*Math.PI/360),Kt=jt/(Vt*it),Wt=P.lerp(ys,Es,P.smoothstep(j,4.2,5)),Xt=zt/(Wt*it*Math.max(.1,ot)),qt=Math.max(Kt,Xt)*(1+Oe*_s);if(Ce.copy(X).addScaledVector(le,qt),s.position.lerp(Ce,t?1:H(2.2)),nt=Math.abs(ge-ae)<5e-4&&s.position.distanceTo(Ce)<.05&&Oe<.001&&I.uAssemble.value>.999,Ae.copy(X),e){const Pe=2*s.position.distanceTo(X)*Math.tan(s.fov*Math.PI/360);ce.subVectors(X,s.position).normalize(),Te.crossVectors(ce,s.up).normalize(),st.crossVectors(Te,ce).normalize();const Yt=P.lerp(Rs,bs,P.smoothstep(j,4,5));Ae.addScaledVector(st,-Pe*Yt)}else{const Pe=2*s.position.distanceTo(X)*Math.tan(s.fov*Math.PI/360)*s.aspect;ce.subVectors(X,s.position).normalize(),Te.crossVectors(ce,s.up).normalize(),Ae.addScaledVector(Te,-Pe*.2)}s.lookAt(Ae),I.uForm.value=j;const at=P.smoothstep(L,.58,.66),ct=P.smoothstep(L,.7,.78);U.uFade.value=at*(1-ct),U.uThreshold.value=P.lerp(-.05,1.5,P.smoothstep(L,.62,.8)),I.uFade.value=1-at*(1-ct)*.75;const lt=P.smoothstep(L,.9,.995);Ie.opacity=lt*.95,ie.visible=lt>.001;const ut=1-P.smoothstep(L,0,.22);E.opacity=.1+ut*.52,pe.scale.setScalar(3+ut*2.8),ie.quaternion.copy(s.quaternion),ie.position.set(0,0,0),Se.rotation.y=A*.12,_e.rotation.y=Se.rotation.y,ne.render()},dispose(){for(const A of o)A.dispose();ne.dispose(),r.dispose()}}}function Ns(){const t=document.createElement("canvas");t.width=128,t.height=128;const e=t.getContext("2d");if(e){const n=e.createRadialGradient(64,64,0,64,64,64);n.addColorStop(0,"rgba(255,255,255,1)"),n.addColorStop(.18,"rgba(255,255,255,0.65)"),n.addColorStop(.45,"rgba(255,255,255,0.16)"),n.addColorStop(1,"rgba(255,255,255,0)"),e.fillStyle=n,e.fillRect(0,0,128,128)}const r=new kn(t);return r.colorSpace=J,r}export{Ps as createJourney};
