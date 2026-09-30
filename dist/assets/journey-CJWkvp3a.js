import{T as on,s as Qe,t as Pt,u as rn,v as Ae,w as Dt,x as V,V as O,e as P,y as X,z as ee,E as an,I as cn,J as ln,K as Ce,f as T,Q as un,X as Ht,Y as hn,Z as kt,_ as Bt,$ as dn,a as F,a0 as fn,a1 as st,a2 as pn,a3 as mn,a4 as gn,a5 as Oe,a6 as Gt,a7 as Je,a8 as An,a9 as Tn,aa as wn,ab as ze,p as xn,ac as Ut,ad as jt,g as ae,ae as vn,B as Te,af as yn,M as Fe,o as zt,ag as bn,ah as Rn,c as Ze,G as Ne,P as Vt,q as C,O as En,ai as Sn,aj as Mn,ak as _n,al as Ln,am as Kt,an as In,ao as yt,ap as bt,aq as Rt,ar as Et,h as St,as as Nn,at as Cn,au as Wt,av as $e,aw as On,W as Fn,l as Pn,S as Dn,ax as Hn,b as Le,A as Ie,ay as kn,az as Bn,aA as Gn,aB as Un}from"./styles-DOu3JOFO.js";import{E as jn,R as zn,U as Vn,O as Kn}from"./index-S2AublcO.js";function Mt(h,t){if(t===on)return console.warn("THREE.BufferGeometryUtils.toTrianglesDrawMode(): Geometry already defined as triangles."),h;if(t===Qe||t===Pt){let e=h.getIndex();if(e===null){const o=[],a=h.getAttribute("position");if(a!==void 0){for(let r=0;r<a.count;r++)o.push(r);h.setIndex(o),e=h.getIndex()}else return console.error("THREE.BufferGeometryUtils.toTrianglesDrawMode(): Undefined position attribute. Processing not possible."),h}const i=e.count-2,n=[];if(t===Qe)for(let o=1;o<=i;o++)n.push(e.getX(0)),n.push(e.getX(o)),n.push(e.getX(o+1));else for(let o=0;o<i;o++)o%2===0?(n.push(e.getX(o)),n.push(e.getX(o+1)),n.push(e.getX(o+2))):(n.push(e.getX(o+2)),n.push(e.getX(o+1)),n.push(e.getX(o)));n.length/3!==i&&console.error("THREE.BufferGeometryUtils.toTrianglesDrawMode(): Unable to generate correct amount of triangles.");const s=h.clone();return s.setIndex(n),s.clearGroups(),s}else return console.error("THREE.BufferGeometryUtils.toTrianglesDrawMode(): Unknown draw mode:",t),h}class Wn extends rn{constructor(t){super(t),this.dracoLoader=null,this.ktx2Loader=null,this.meshoptDecoder=null,this.pluginCallbacks=[],this.register(function(e){return new Jn(e)}),this.register(function(e){return new Zn(e)}),this.register(function(e){return new as(e)}),this.register(function(e){return new cs(e)}),this.register(function(e){return new ls(e)}),this.register(function(e){return new es(e)}),this.register(function(e){return new ts(e)}),this.register(function(e){return new ns(e)}),this.register(function(e){return new ss(e)}),this.register(function(e){return new Qn(e)}),this.register(function(e){return new os(e)}),this.register(function(e){return new $n(e)}),this.register(function(e){return new rs(e)}),this.register(function(e){return new is(e)}),this.register(function(e){return new qn(e)}),this.register(function(e){return new us(e)}),this.register(function(e){return new hs(e)})}load(t,e,i,n){const s=this;let o;if(this.resourcePath!=="")o=this.resourcePath;else if(this.path!==""){const c=Ae.extractUrlBase(t);o=Ae.resolveURL(c,this.path)}else o=Ae.extractUrlBase(t);this.manager.itemStart(t);const a=function(c){n?n(c):console.error(c),s.manager.itemError(t),s.manager.itemEnd(t)},r=new Dt(this.manager);r.setPath(this.path),r.setResponseType("arraybuffer"),r.setRequestHeader(this.requestHeader),r.setWithCredentials(this.withCredentials),r.load(t,function(c){try{s.parse(c,o,function(u){e(u),s.manager.itemEnd(t)},a)}catch(u){a(u)}},i,a)}setDRACOLoader(t){return this.dracoLoader=t,this}setKTX2Loader(t){return this.ktx2Loader=t,this}setMeshoptDecoder(t){return this.meshoptDecoder=t,this}register(t){return this.pluginCallbacks.indexOf(t)===-1&&this.pluginCallbacks.push(t),this}unregister(t){return this.pluginCallbacks.indexOf(t)!==-1&&this.pluginCallbacks.splice(this.pluginCallbacks.indexOf(t),1),this}parse(t,e,i,n){let s;const o={},a={},r=new TextDecoder;if(typeof t=="string")s=JSON.parse(t);else if(t instanceof ArrayBuffer)if(r.decode(new Uint8Array(t,0,4))===Xt){try{o[w.KHR_BINARY_GLTF]=new ds(t)}catch(l){n&&n(l);return}s=JSON.parse(o[w.KHR_BINARY_GLTF].content)}else s=JSON.parse(r.decode(t));else s=t;if(s.asset===void 0||s.asset.version[0]<2){n&&n(new Error("THREE.GLTFLoader: Unsupported asset. glTF versions >=2.0 are supported."));return}const c=new Es(s,{path:e||this.resourcePath||"",crossOrigin:this.crossOrigin,requestHeader:this.requestHeader,manager:this.manager,ktx2Loader:this.ktx2Loader,meshoptDecoder:this.meshoptDecoder});c.fileLoader.setRequestHeader(this.requestHeader);for(let u=0;u<this.pluginCallbacks.length;u++){const l=this.pluginCallbacks[u](c);l.name||console.error("THREE.GLTFLoader: Invalid plugin found: missing name"),a[l.name]=l,o[l.name]=!0}if(s.extensionsUsed)for(let u=0;u<s.extensionsUsed.length;++u){const l=s.extensionsUsed[u],d=s.extensionsRequired||[];switch(l){case w.KHR_MATERIALS_UNLIT:o[l]=new Yn;break;case w.KHR_DRACO_MESH_COMPRESSION:o[l]=new fs(s,this.dracoLoader);break;case w.KHR_TEXTURE_TRANSFORM:o[l]=new ps;break;case w.KHR_MESH_QUANTIZATION:o[l]=new ms;break;default:d.indexOf(l)>=0&&a[l]===void 0&&console.warn('THREE.GLTFLoader: Unknown extension "'+l+'".')}}c.setExtensions(o),c.setPlugins(a),c.parse(i,n)}parseAsync(t,e){const i=this;return new Promise(function(n,s){i.parse(t,e,n,s)})}}function Xn(){let h={};return{get:function(t){return h[t]},add:function(t,e){h[t]=e},remove:function(t){delete h[t]},removeAll:function(){h={}}}}const w={KHR_BINARY_GLTF:"KHR_binary_glTF",KHR_DRACO_MESH_COMPRESSION:"KHR_draco_mesh_compression",KHR_LIGHTS_PUNCTUAL:"KHR_lights_punctual",KHR_MATERIALS_CLEARCOAT:"KHR_materials_clearcoat",KHR_MATERIALS_DISPERSION:"KHR_materials_dispersion",KHR_MATERIALS_IOR:"KHR_materials_ior",KHR_MATERIALS_SHEEN:"KHR_materials_sheen",KHR_MATERIALS_SPECULAR:"KHR_materials_specular",KHR_MATERIALS_TRANSMISSION:"KHR_materials_transmission",KHR_MATERIALS_IRIDESCENCE:"KHR_materials_iridescence",KHR_MATERIALS_ANISOTROPY:"KHR_materials_anisotropy",KHR_MATERIALS_UNLIT:"KHR_materials_unlit",KHR_MATERIALS_VOLUME:"KHR_materials_volume",KHR_TEXTURE_BASISU:"KHR_texture_basisu",KHR_TEXTURE_TRANSFORM:"KHR_texture_transform",KHR_MESH_QUANTIZATION:"KHR_mesh_quantization",KHR_MATERIALS_EMISSIVE_STRENGTH:"KHR_materials_emissive_strength",EXT_MATERIALS_BUMP:"EXT_materials_bump",EXT_TEXTURE_WEBP:"EXT_texture_webp",EXT_TEXTURE_AVIF:"EXT_texture_avif",EXT_MESHOPT_COMPRESSION:"EXT_meshopt_compression",EXT_MESH_GPU_INSTANCING:"EXT_mesh_gpu_instancing"};class qn{constructor(t){this.parser=t,this.name=w.KHR_LIGHTS_PUNCTUAL,this.cache={refs:{},uses:{}}}_markDefs(){const t=this.parser,e=this.parser.json.nodes||[];for(let i=0,n=e.length;i<n;i++){const s=e[i];s.extensions&&s.extensions[this.name]&&s.extensions[this.name].light!==void 0&&t._addNodeRef(this.cache,s.extensions[this.name].light)}}_loadLight(t){const e=this.parser,i="light:"+t;let n=e.cache.get(i);if(n)return n;const s=e.json,r=((s.extensions&&s.extensions[this.name]||{}).lights||[])[t];let c;const u=new P(16777215);r.color!==void 0&&u.setRGB(r.color[0],r.color[1],r.color[2],X);const l=r.range!==void 0?r.range:0;switch(r.type){case"directional":c=new ln(u),c.target.position.set(0,0,-1),c.add(c.target);break;case"point":c=new cn(u),c.distance=l;break;case"spot":c=new an(u),c.distance=l,r.spot=r.spot||{},r.spot.innerConeAngle=r.spot.innerConeAngle!==void 0?r.spot.innerConeAngle:0,r.spot.outerConeAngle=r.spot.outerConeAngle!==void 0?r.spot.outerConeAngle:Math.PI/4,c.angle=r.spot.outerConeAngle,c.penumbra=1-r.spot.innerConeAngle/r.spot.outerConeAngle,c.target.position.set(0,0,-1),c.add(c.target);break;default:throw new Error("THREE.GLTFLoader: Unexpected light type: "+r.type)}return c.position.set(0,0,0),c.decay=2,W(c,r),r.intensity!==void 0&&(c.intensity=r.intensity),c.name=e.createUniqueName(r.name||"light_"+t),n=Promise.resolve(c),e.cache.add(i,n),n}getDependency(t,e){if(t==="light")return this._loadLight(e)}createNodeAttachment(t){const e=this,i=this.parser,s=i.json.nodes[t],a=(s.extensions&&s.extensions[this.name]||{}).light;return a===void 0?null:this._loadLight(a).then(function(r){return i._getNodeRef(e.cache,a,r)})}}class Yn{constructor(){this.name=w.KHR_MATERIALS_UNLIT}getMaterialType(){return ae}extendParams(t,e,i){const n=[];t.color=new P(1,1,1),t.opacity=1;const s=e.pbrMetallicRoughness;if(s){if(Array.isArray(s.baseColorFactor)){const o=s.baseColorFactor;t.color.setRGB(o[0],o[1],o[2],X),t.opacity=o[3]}s.baseColorTexture!==void 0&&n.push(i.assignTexture(t,"map",s.baseColorTexture,ee))}return Promise.all(n)}}class Qn{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_EMISSIVE_STRENGTH}extendMaterialParams(t,e){const n=this.parser.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=n.extensions[this.name].emissiveStrength;return s!==void 0&&(e.emissiveIntensity=s),Promise.resolve()}}class Jn{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_CLEARCOAT}getMaterialType(t){const i=this.parser.json.materials[t];return!i.extensions||!i.extensions[this.name]?null:V}extendMaterialParams(t,e){const i=this.parser,n=i.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[],o=n.extensions[this.name];if(o.clearcoatFactor!==void 0&&(e.clearcoat=o.clearcoatFactor),o.clearcoatTexture!==void 0&&s.push(i.assignTexture(e,"clearcoatMap",o.clearcoatTexture)),o.clearcoatRoughnessFactor!==void 0&&(e.clearcoatRoughness=o.clearcoatRoughnessFactor),o.clearcoatRoughnessTexture!==void 0&&s.push(i.assignTexture(e,"clearcoatRoughnessMap",o.clearcoatRoughnessTexture)),o.clearcoatNormalTexture!==void 0&&(s.push(i.assignTexture(e,"clearcoatNormalMap",o.clearcoatNormalTexture)),o.clearcoatNormalTexture.scale!==void 0)){const a=o.clearcoatNormalTexture.scale;e.clearcoatNormalScale=new O(a,a)}return Promise.all(s)}}class Zn{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_DISPERSION}getMaterialType(t){const i=this.parser.json.materials[t];return!i.extensions||!i.extensions[this.name]?null:V}extendMaterialParams(t,e){const n=this.parser.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=n.extensions[this.name];return e.dispersion=s.dispersion!==void 0?s.dispersion:0,Promise.resolve()}}class $n{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_IRIDESCENCE}getMaterialType(t){const i=this.parser.json.materials[t];return!i.extensions||!i.extensions[this.name]?null:V}extendMaterialParams(t,e){const i=this.parser,n=i.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[],o=n.extensions[this.name];return o.iridescenceFactor!==void 0&&(e.iridescence=o.iridescenceFactor),o.iridescenceTexture!==void 0&&s.push(i.assignTexture(e,"iridescenceMap",o.iridescenceTexture)),o.iridescenceIor!==void 0&&(e.iridescenceIOR=o.iridescenceIor),e.iridescenceThicknessRange===void 0&&(e.iridescenceThicknessRange=[100,400]),o.iridescenceThicknessMinimum!==void 0&&(e.iridescenceThicknessRange[0]=o.iridescenceThicknessMinimum),o.iridescenceThicknessMaximum!==void 0&&(e.iridescenceThicknessRange[1]=o.iridescenceThicknessMaximum),o.iridescenceThicknessTexture!==void 0&&s.push(i.assignTexture(e,"iridescenceThicknessMap",o.iridescenceThicknessTexture)),Promise.all(s)}}class es{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_SHEEN}getMaterialType(t){const i=this.parser.json.materials[t];return!i.extensions||!i.extensions[this.name]?null:V}extendMaterialParams(t,e){const i=this.parser,n=i.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[];e.sheenColor=new P(0,0,0),e.sheenRoughness=0,e.sheen=1;const o=n.extensions[this.name];if(o.sheenColorFactor!==void 0){const a=o.sheenColorFactor;e.sheenColor.setRGB(a[0],a[1],a[2],X)}return o.sheenRoughnessFactor!==void 0&&(e.sheenRoughness=o.sheenRoughnessFactor),o.sheenColorTexture!==void 0&&s.push(i.assignTexture(e,"sheenColorMap",o.sheenColorTexture,ee)),o.sheenRoughnessTexture!==void 0&&s.push(i.assignTexture(e,"sheenRoughnessMap",o.sheenRoughnessTexture)),Promise.all(s)}}class ts{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_TRANSMISSION}getMaterialType(t){const i=this.parser.json.materials[t];return!i.extensions||!i.extensions[this.name]?null:V}extendMaterialParams(t,e){const i=this.parser,n=i.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[],o=n.extensions[this.name];return o.transmissionFactor!==void 0&&(e.transmission=o.transmissionFactor),o.transmissionTexture!==void 0&&s.push(i.assignTexture(e,"transmissionMap",o.transmissionTexture)),Promise.all(s)}}class ns{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_VOLUME}getMaterialType(t){const i=this.parser.json.materials[t];return!i.extensions||!i.extensions[this.name]?null:V}extendMaterialParams(t,e){const i=this.parser,n=i.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[],o=n.extensions[this.name];e.thickness=o.thicknessFactor!==void 0?o.thicknessFactor:0,o.thicknessTexture!==void 0&&s.push(i.assignTexture(e,"thicknessMap",o.thicknessTexture)),e.attenuationDistance=o.attenuationDistance||1/0;const a=o.attenuationColor||[1,1,1];return e.attenuationColor=new P().setRGB(a[0],a[1],a[2],X),Promise.all(s)}}class ss{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_IOR}getMaterialType(t){const i=this.parser.json.materials[t];return!i.extensions||!i.extensions[this.name]?null:V}extendMaterialParams(t,e){const n=this.parser.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=n.extensions[this.name];return e.ior=s.ior!==void 0?s.ior:1.5,Promise.resolve()}}class os{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_SPECULAR}getMaterialType(t){const i=this.parser.json.materials[t];return!i.extensions||!i.extensions[this.name]?null:V}extendMaterialParams(t,e){const i=this.parser,n=i.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[],o=n.extensions[this.name];e.specularIntensity=o.specularFactor!==void 0?o.specularFactor:1,o.specularTexture!==void 0&&s.push(i.assignTexture(e,"specularIntensityMap",o.specularTexture));const a=o.specularColorFactor||[1,1,1];return e.specularColor=new P().setRGB(a[0],a[1],a[2],X),o.specularColorTexture!==void 0&&s.push(i.assignTexture(e,"specularColorMap",o.specularColorTexture,ee)),Promise.all(s)}}class is{constructor(t){this.parser=t,this.name=w.EXT_MATERIALS_BUMP}getMaterialType(t){const i=this.parser.json.materials[t];return!i.extensions||!i.extensions[this.name]?null:V}extendMaterialParams(t,e){const i=this.parser,n=i.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[],o=n.extensions[this.name];return e.bumpScale=o.bumpFactor!==void 0?o.bumpFactor:1,o.bumpTexture!==void 0&&s.push(i.assignTexture(e,"bumpMap",o.bumpTexture)),Promise.all(s)}}class rs{constructor(t){this.parser=t,this.name=w.KHR_MATERIALS_ANISOTROPY}getMaterialType(t){const i=this.parser.json.materials[t];return!i.extensions||!i.extensions[this.name]?null:V}extendMaterialParams(t,e){const i=this.parser,n=i.json.materials[t];if(!n.extensions||!n.extensions[this.name])return Promise.resolve();const s=[],o=n.extensions[this.name];return o.anisotropyStrength!==void 0&&(e.anisotropy=o.anisotropyStrength),o.anisotropyRotation!==void 0&&(e.anisotropyRotation=o.anisotropyRotation),o.anisotropyTexture!==void 0&&s.push(i.assignTexture(e,"anisotropyMap",o.anisotropyTexture)),Promise.all(s)}}class as{constructor(t){this.parser=t,this.name=w.KHR_TEXTURE_BASISU}loadTexture(t){const e=this.parser,i=e.json,n=i.textures[t];if(!n.extensions||!n.extensions[this.name])return null;const s=n.extensions[this.name],o=e.options.ktx2Loader;if(!o){if(i.extensionsRequired&&i.extensionsRequired.indexOf(this.name)>=0)throw new Error("THREE.GLTFLoader: setKTX2Loader must be called before loading KTX2 textures");return null}return e.loadTextureImage(t,s.source,o)}}class cs{constructor(t){this.parser=t,this.name=w.EXT_TEXTURE_WEBP,this.isSupported=null}loadTexture(t){const e=this.name,i=this.parser,n=i.json,s=n.textures[t];if(!s.extensions||!s.extensions[e])return null;const o=s.extensions[e],a=n.images[o.source];let r=i.textureLoader;if(a.uri){const c=i.options.manager.getHandler(a.uri);c!==null&&(r=c)}return this.detectSupport().then(function(c){if(c)return i.loadTextureImage(t,o.source,r);if(n.extensionsRequired&&n.extensionsRequired.indexOf(e)>=0)throw new Error("THREE.GLTFLoader: WebP required by asset but unsupported.");return i.loadTexture(t)})}detectSupport(){return this.isSupported||(this.isSupported=new Promise(function(t){const e=new Image;e.src="data:image/webp;base64,UklGRiIAAABXRUJQVlA4IBYAAAAwAQCdASoBAAEADsD+JaQAA3AAAAAA",e.onload=e.onerror=function(){t(e.height===1)}})),this.isSupported}}class ls{constructor(t){this.parser=t,this.name=w.EXT_TEXTURE_AVIF,this.isSupported=null}loadTexture(t){const e=this.name,i=this.parser,n=i.json,s=n.textures[t];if(!s.extensions||!s.extensions[e])return null;const o=s.extensions[e],a=n.images[o.source];let r=i.textureLoader;if(a.uri){const c=i.options.manager.getHandler(a.uri);c!==null&&(r=c)}return this.detectSupport().then(function(c){if(c)return i.loadTextureImage(t,o.source,r);if(n.extensionsRequired&&n.extensionsRequired.indexOf(e)>=0)throw new Error("THREE.GLTFLoader: AVIF required by asset but unsupported.");return i.loadTexture(t)})}detectSupport(){return this.isSupported||(this.isSupported=new Promise(function(t){const e=new Image;e.src="data:image/avif;base64,AAAAIGZ0eXBhdmlmAAAAAGF2aWZtaWYxbWlhZk1BMUIAAADybWV0YQAAAAAAAAAoaGRscgAAAAAAAAAAcGljdAAAAAAAAAAAAAAAAGxpYmF2aWYAAAAADnBpdG0AAAAAAAEAAAAeaWxvYwAAAABEAAABAAEAAAABAAABGgAAABcAAAAoaWluZgAAAAAAAQAAABppbmZlAgAAAAABAABhdjAxQ29sb3IAAAAAamlwcnAAAABLaXBjbwAAABRpc3BlAAAAAAAAAAEAAAABAAAAEHBpeGkAAAAAAwgICAAAAAxhdjFDgQAMAAAAABNjb2xybmNseAACAAIABoAAAAAXaXBtYQAAAAAAAAABAAEEAQKDBAAAAB9tZGF0EgAKCBgABogQEDQgMgkQAAAAB8dSLfI=",e.onload=e.onerror=function(){t(e.height===1)}})),this.isSupported}}class us{constructor(t){this.name=w.EXT_MESHOPT_COMPRESSION,this.parser=t}loadBufferView(t){const e=this.parser.json,i=e.bufferViews[t];if(i.extensions&&i.extensions[this.name]){const n=i.extensions[this.name],s=this.parser.getDependency("buffer",n.buffer),o=this.parser.options.meshoptDecoder;if(!o||!o.supported){if(e.extensionsRequired&&e.extensionsRequired.indexOf(this.name)>=0)throw new Error("THREE.GLTFLoader: setMeshoptDecoder must be called before loading compressed files");return null}return s.then(function(a){const r=n.byteOffset||0,c=n.byteLength||0,u=n.count,l=n.byteStride,d=new Uint8Array(a,r,c);return o.decodeGltfBufferAsync?o.decodeGltfBufferAsync(u,l,d,n.mode,n.filter).then(function(f){return f.buffer}):o.ready.then(function(){const f=new ArrayBuffer(u*l);return o.decodeGltfBuffer(new Uint8Array(f),u,l,d,n.mode,n.filter),f})})}else return null}}class hs{constructor(t){this.name=w.EXT_MESH_GPU_INSTANCING,this.parser=t}createNodeMesh(t){const e=this.parser.json,i=e.nodes[t];if(!i.extensions||!i.extensions[this.name]||i.mesh===void 0)return null;const n=e.meshes[i.mesh];for(const c of n.primitives)if(c.mode!==k.TRIANGLES&&c.mode!==k.TRIANGLE_STRIP&&c.mode!==k.TRIANGLE_FAN&&c.mode!==void 0)return null;const o=i.extensions[this.name].attributes,a=[],r={};for(const c in o)a.push(this.parser.getDependency("accessor",o[c]).then(u=>(r[c]=u,r[c])));return a.length<1?null:(a.push(this.parser.createNodeMesh(t)),Promise.all(a).then(c=>{const u=c.pop(),l=u.isGroup?u.children:[u],d=c[0].count,f=[];for(const m of l){const x=new Ce,p=new T,g=new Ht,y=new T(1,1,1),R=new un(m.geometry,m.material,d);for(let v=0;v<d;v++)r.TRANSLATION&&p.fromBufferAttribute(r.TRANSLATION,v),r.ROTATION&&g.fromBufferAttribute(r.ROTATION,v),r.SCALE&&y.fromBufferAttribute(r.SCALE,v),R.setMatrixAt(v,x.compose(p,g,y));for(const v in r)if(v==="_COLOR_0"){const S=r[v];R.instanceColor=new hn(S.array,S.itemSize,S.normalized)}else v!=="TRANSLATION"&&v!=="ROTATION"&&v!=="SCALE"&&m.geometry.setAttribute(v,r[v]);kt.prototype.copy.call(R,m),this.parser.assignFinalMaterial(R),f.push(R)}return u.isGroup?(u.clear(),u.add(...f),u):f[0]}))}}const Xt="glTF",me=12,_t={JSON:1313821514,BIN:5130562};class ds{constructor(t){this.name=w.KHR_BINARY_GLTF,this.content=null,this.body=null;const e=new DataView(t,0,me),i=new TextDecoder;if(this.header={magic:i.decode(new Uint8Array(t.slice(0,4))),version:e.getUint32(4,!0),length:e.getUint32(8,!0)},this.header.magic!==Xt)throw new Error("THREE.GLTFLoader: Unsupported glTF-Binary header.");if(this.header.version<2)throw new Error("THREE.GLTFLoader: Legacy binary file detected.");const n=this.header.length-me,s=new DataView(t,me);let o=0;for(;o<n;){const a=s.getUint32(o,!0);o+=4;const r=s.getUint32(o,!0);if(o+=4,r===_t.JSON){const c=new Uint8Array(t,me+o,a);this.content=i.decode(c)}else if(r===_t.BIN){const c=me+o;this.body=t.slice(c,c+a)}o+=a}if(this.content===null)throw new Error("THREE.GLTFLoader: JSON content not found.")}}class fs{constructor(t,e){if(!e)throw new Error("THREE.GLTFLoader: No DRACOLoader instance provided.");this.name=w.KHR_DRACO_MESH_COMPRESSION,this.json=t,this.dracoLoader=e,this.dracoLoader.preload()}decodePrimitive(t,e){const i=this.json,n=this.dracoLoader,s=t.extensions[this.name].bufferView,o=t.extensions[this.name].attributes,a={},r={},c={};for(const u in o){const l=et[u]||u.toLowerCase();a[l]=o[u]}for(const u in t.attributes){const l=et[u]||u.toLowerCase();if(o[u]!==void 0){const d=i.accessors[t.attributes[u]],f=ce[d.componentType];c[l]=f.name,r[l]=d.normalized===!0}}return e.getDependency("bufferView",s).then(function(u){return new Promise(function(l,d){n.decodeDracoFile(u,function(f){for(const m in f.attributes){const x=f.attributes[m],p=r[m];p!==void 0&&(x.normalized=p)}l(f)},a,c,X,d)})})}}class ps{constructor(){this.name=w.KHR_TEXTURE_TRANSFORM}extendTexture(t,e){return(e.texCoord===void 0||e.texCoord===t.channel)&&e.offset===void 0&&e.rotation===void 0&&e.scale===void 0||(t=t.clone(),e.texCoord!==void 0&&(t.channel=e.texCoord),e.offset!==void 0&&t.offset.fromArray(e.offset),e.rotation!==void 0&&(t.rotation=e.rotation),e.scale!==void 0&&t.repeat.fromArray(e.scale),t.needsUpdate=!0),t}}class ms{constructor(){this.name=w.KHR_MESH_QUANTIZATION}}class qt extends Cn{constructor(t,e,i,n){super(t,e,i,n)}copySampleValue_(t){const e=this.resultBuffer,i=this.sampleValues,n=this.valueSize,s=t*n*3+n;for(let o=0;o!==n;o++)e[o]=i[s+o];return e}interpolate_(t,e,i,n){const s=this.resultBuffer,o=this.sampleValues,a=this.valueSize,r=a*2,c=a*3,u=n-e,l=(i-e)/u,d=l*l,f=d*l,m=t*c,x=m-c,p=-2*f+3*d,g=f-d,y=1-p,R=g-d+l;for(let v=0;v!==a;v++){const S=o[x+v+a],M=o[x+v+r]*u,E=o[m+v+a],I=o[m+v]*u;s[v]=y*S+R*M+p*E+g*I}return s}}const gs=new Ht;class As extends qt{interpolate_(t,e,i,n){const s=super.interpolate_(t,e,i,n);return gs.fromArray(s).normalize().toArray(s),s}}const k={POINTS:0,LINES:1,LINE_LOOP:2,LINE_STRIP:3,TRIANGLES:4,TRIANGLE_STRIP:5,TRIANGLE_FAN:6},ce={5120:Int8Array,5121:Uint8Array,5122:Int16Array,5123:Uint16Array,5125:Uint32Array,5126:Float32Array},Lt={9728:Gt,9729:Oe,9984:gn,9985:mn,9986:pn,9987:st},It={33071:Tn,33648:An,10497:Je},Ve={SCALAR:1,VEC2:2,VEC3:3,VEC4:4,MAT2:4,MAT3:9,MAT4:16},et={POSITION:"position",NORMAL:"normal",TANGENT:"tangent",TEXCOORD_0:"uv",TEXCOORD_1:"uv1",TEXCOORD_2:"uv2",TEXCOORD_3:"uv3",COLOR_0:"color",WEIGHTS_0:"skinWeight",JOINTS_0:"skinIndex"},q={scale:"scale",translation:"position",rotation:"quaternion",weights:"morphTargetInfluences"},Ts={CUBICSPLINE:void 0,LINEAR:Kt,STEP:Ln},Ke={OPAQUE:"OPAQUE",MASK:"MASK",BLEND:"BLEND"};function ws(h){return h.DefaultMaterial===void 0&&(h.DefaultMaterial=new Ut({color:16777215,emissive:0,metalness:1,roughness:1,transparent:!1,depthTest:!0,side:Nn})),h.DefaultMaterial}function $(h,t,e){for(const i in e.extensions)h[i]===void 0&&(t.userData.gltfExtensions=t.userData.gltfExtensions||{},t.userData.gltfExtensions[i]=e.extensions[i])}function W(h,t){t.extras!==void 0&&(typeof t.extras=="object"?Object.assign(h.userData,t.extras):console.warn("THREE.GLTFLoader: Ignoring primitive type .extras, "+t.extras))}function xs(h,t,e){let i=!1,n=!1,s=!1;for(let c=0,u=t.length;c<u;c++){const l=t[c];if(l.POSITION!==void 0&&(i=!0),l.NORMAL!==void 0&&(n=!0),l.COLOR_0!==void 0&&(s=!0),i&&n&&s)break}if(!i&&!n&&!s)return Promise.resolve(h);const o=[],a=[],r=[];for(let c=0,u=t.length;c<u;c++){const l=t[c];if(i){const d=l.POSITION!==void 0?e.getDependency("accessor",l.POSITION):h.attributes.position;o.push(d)}if(n){const d=l.NORMAL!==void 0?e.getDependency("accessor",l.NORMAL):h.attributes.normal;a.push(d)}if(s){const d=l.COLOR_0!==void 0?e.getDependency("accessor",l.COLOR_0):h.attributes.color;r.push(d)}}return Promise.all([Promise.all(o),Promise.all(a),Promise.all(r)]).then(function(c){const u=c[0],l=c[1],d=c[2];return i&&(h.morphAttributes.position=u),n&&(h.morphAttributes.normal=l),s&&(h.morphAttributes.color=d),h.morphTargetsRelative=!0,h})}function vs(h,t){if(h.updateMorphTargets(),t.weights!==void 0)for(let e=0,i=t.weights.length;e<i;e++)h.morphTargetInfluences[e]=t.weights[e];if(t.extras&&Array.isArray(t.extras.targetNames)){const e=t.extras.targetNames;if(h.morphTargetInfluences.length===e.length){h.morphTargetDictionary={};for(let i=0,n=e.length;i<n;i++)h.morphTargetDictionary[e[i]]=i}else console.warn("THREE.GLTFLoader: Invalid extras.targetNames length. Ignoring names.")}}function ys(h){let t;const e=h.extensions&&h.extensions[w.KHR_DRACO_MESH_COMPRESSION];if(e?t="draco:"+e.bufferView+":"+e.indices+":"+We(e.attributes):t=h.indices+":"+We(h.attributes)+":"+h.mode,h.targets!==void 0)for(let i=0,n=h.targets.length;i<n;i++)t+=":"+We(h.targets[i]);return t}function We(h){let t="";const e=Object.keys(h).sort();for(let i=0,n=e.length;i<n;i++)t+=e[i]+":"+h[e[i]]+";";return t}function tt(h){switch(h){case Int8Array:return 1/127;case Uint8Array:return 1/255;case Int16Array:return 1/32767;case Uint16Array:return 1/65535;default:throw new Error("THREE.GLTFLoader: Unsupported normalized accessor component type.")}}function bs(h){return h.search(/\.jpe?g($|\?)/i)>0||h.search(/^data\:image\/jpeg/)===0?"image/jpeg":h.search(/\.webp($|\?)/i)>0||h.search(/^data\:image\/webp/)===0?"image/webp":h.search(/\.ktx2($|\?)/i)>0||h.search(/^data\:image\/ktx2/)===0?"image/ktx2":"image/png"}const Rs=new Ce;class Es{constructor(t={},e={}){this.json=t,this.extensions={},this.plugins={},this.options=e,this.cache=new Xn,this.associations=new Map,this.primitiveCache={},this.nodeCache={},this.meshCache={refs:{},uses:{}},this.cameraCache={refs:{},uses:{}},this.lightCache={refs:{},uses:{}},this.sourceCache={},this.textureCache={},this.nodeNamesUsed={};let i=!1,n=-1,s=!1,o=-1;if(typeof navigator<"u"){const a=navigator.userAgent;i=/^((?!chrome|android).)*safari/i.test(a)===!0;const r=a.match(/Version\/(\d+)/);n=i&&r?parseInt(r[1],10):-1,s=a.indexOf("Firefox")>-1,o=s?a.match(/Firefox\/([0-9]+)\./)[1]:-1}typeof createImageBitmap>"u"||i&&n<17||s&&o<98?this.textureLoader=new Bt(this.options.manager):this.textureLoader=new dn(this.options.manager),this.textureLoader.setCrossOrigin(this.options.crossOrigin),this.textureLoader.setRequestHeader(this.options.requestHeader),this.fileLoader=new Dt(this.options.manager),this.fileLoader.setResponseType("arraybuffer"),this.options.crossOrigin==="use-credentials"&&this.fileLoader.setWithCredentials(!0)}setExtensions(t){this.extensions=t}setPlugins(t){this.plugins=t}parse(t,e){const i=this,n=this.json,s=this.extensions;this.cache.removeAll(),this.nodeCache={},this._invokeAll(function(o){return o._markDefs&&o._markDefs()}),Promise.all(this._invokeAll(function(o){return o.beforeRoot&&o.beforeRoot()})).then(function(){return Promise.all([i.getDependencies("scene"),i.getDependencies("animation"),i.getDependencies("camera")])}).then(function(o){const a={scene:o[0][n.scene||0],scenes:o[0],animations:o[1],cameras:o[2],asset:n.asset,parser:i,userData:{}};return $(s,a,n),W(a,n),Promise.all(i._invokeAll(function(r){return r.afterRoot&&r.afterRoot(a)})).then(function(){for(const r of a.scenes)r.updateMatrixWorld();t(a)})}).catch(e)}_markDefs(){const t=this.json.nodes||[],e=this.json.skins||[],i=this.json.meshes||[];for(let n=0,s=e.length;n<s;n++){const o=e[n].joints;for(let a=0,r=o.length;a<r;a++)t[o[a]].isBone=!0}for(let n=0,s=t.length;n<s;n++){const o=t[n];o.mesh!==void 0&&(this._addNodeRef(this.meshCache,o.mesh),o.skin!==void 0&&(i[o.mesh].isSkinnedMesh=!0)),o.camera!==void 0&&this._addNodeRef(this.cameraCache,o.camera)}}_addNodeRef(t,e){e!==void 0&&(t.refs[e]===void 0&&(t.refs[e]=t.uses[e]=0),t.refs[e]++)}_getNodeRef(t,e,i){if(t.refs[e]<=1)return i;const n=i.clone(),s=(o,a)=>{const r=this.associations.get(o);r!=null&&this.associations.set(a,r);for(const[c,u]of o.children.entries())s(u,a.children[c])};return s(i,n),n.name+="_instance_"+t.uses[e]++,n}_invokeOne(t){const e=Object.values(this.plugins);e.push(this);for(let i=0;i<e.length;i++){const n=t(e[i]);if(n)return n}return null}_invokeAll(t){const e=Object.values(this.plugins);e.unshift(this);const i=[];for(let n=0;n<e.length;n++){const s=t(e[n]);s&&i.push(s)}return i}getDependency(t,e){const i=t+":"+e;let n=this.cache.get(i);if(!n){switch(t){case"scene":n=this.loadScene(e);break;case"node":n=this._invokeOne(function(s){return s.loadNode&&s.loadNode(e)});break;case"mesh":n=this._invokeOne(function(s){return s.loadMesh&&s.loadMesh(e)});break;case"accessor":n=this.loadAccessor(e);break;case"bufferView":n=this._invokeOne(function(s){return s.loadBufferView&&s.loadBufferView(e)});break;case"buffer":n=this.loadBuffer(e);break;case"material":n=this._invokeOne(function(s){return s.loadMaterial&&s.loadMaterial(e)});break;case"texture":n=this._invokeOne(function(s){return s.loadTexture&&s.loadTexture(e)});break;case"skin":n=this.loadSkin(e);break;case"animation":n=this._invokeOne(function(s){return s.loadAnimation&&s.loadAnimation(e)});break;case"camera":n=this.loadCamera(e);break;default:if(n=this._invokeOne(function(s){return s!=this&&s.getDependency&&s.getDependency(t,e)}),!n)throw new Error("Unknown type: "+t);break}this.cache.add(i,n)}return n}getDependencies(t){let e=this.cache.get(t);if(!e){const i=this,n=this.json[t+(t==="mesh"?"es":"s")]||[];e=Promise.all(n.map(function(s,o){return i.getDependency(t,o)})),this.cache.add(t,e)}return e}loadBuffer(t){const e=this.json.buffers[t],i=this.fileLoader;if(e.type&&e.type!=="arraybuffer")throw new Error("THREE.GLTFLoader: "+e.type+" buffer type is not supported.");if(e.uri===void 0&&t===0)return Promise.resolve(this.extensions[w.KHR_BINARY_GLTF].body);const n=this.options;return new Promise(function(s,o){i.load(Ae.resolveURL(e.uri,n.path),s,void 0,function(){o(new Error('THREE.GLTFLoader: Failed to load buffer "'+e.uri+'".'))})})}loadBufferView(t){const e=this.json.bufferViews[t];return this.getDependency("buffer",e.buffer).then(function(i){const n=e.byteLength||0,s=e.byteOffset||0;return i.slice(s,s+n)})}loadAccessor(t){const e=this,i=this.json,n=this.json.accessors[t];if(n.bufferView===void 0&&n.sparse===void 0){const o=Ve[n.type],a=ce[n.componentType],r=n.normalized===!0,c=new a(n.count*o);return Promise.resolve(new F(c,o,r))}const s=[];return n.bufferView!==void 0?s.push(this.getDependency("bufferView",n.bufferView)):s.push(null),n.sparse!==void 0&&(s.push(this.getDependency("bufferView",n.sparse.indices.bufferView)),s.push(this.getDependency("bufferView",n.sparse.values.bufferView))),Promise.all(s).then(function(o){const a=o[0],r=Ve[n.type],c=ce[n.componentType],u=c.BYTES_PER_ELEMENT,l=u*r,d=n.byteOffset||0,f=n.bufferView!==void 0?i.bufferViews[n.bufferView].byteStride:void 0,m=n.normalized===!0;let x,p;if(f&&f!==l){const g=Math.floor(d/f),y="InterleavedBuffer:"+n.bufferView+":"+n.componentType+":"+g+":"+n.count;let R=e.cache.get(y);R||(x=new c(a,g*f,n.count*f/u),R=new fn(x,f/u),e.cache.add(y,R)),p=new In(R,r,d%f/u,m)}else a===null?x=new c(n.count*r):x=new c(a,d,n.count*r),p=new F(x,r,m);if(n.sparse!==void 0){const g=Ve.SCALAR,y=ce[n.sparse.indices.componentType],R=n.sparse.indices.byteOffset||0,v=n.sparse.values.byteOffset||0,S=new y(o[1],R,n.sparse.count*g),M=new c(o[2],v,n.sparse.count*r);a!==null&&(p=new F(p.array.slice(),p.itemSize,p.normalized)),p.normalized=!1;for(let E=0,I=S.length;E<I;E++){const N=S[E];if(p.setX(N,M[E*r]),r>=2&&p.setY(N,M[E*r+1]),r>=3&&p.setZ(N,M[E*r+2]),r>=4&&p.setW(N,M[E*r+3]),r>=5)throw new Error("THREE.GLTFLoader: Unsupported itemSize in sparse BufferAttribute.")}p.normalized=m}return p})}loadTexture(t){const e=this.json,i=this.options,s=e.textures[t].source,o=e.images[s];let a=this.textureLoader;if(o.uri){const r=i.manager.getHandler(o.uri);r!==null&&(a=r)}return this.loadTextureImage(t,s,a)}loadTextureImage(t,e,i){const n=this,s=this.json,o=s.textures[t],a=s.images[e],r=(a.uri||a.bufferView)+":"+o.sampler;if(this.textureCache[r])return this.textureCache[r];const c=this.loadImageSource(e,i).then(function(u){u.flipY=!1,u.name=o.name||a.name||"",u.name===""&&typeof a.uri=="string"&&a.uri.startsWith("data:image/")===!1&&(u.name=a.uri);const d=(s.samplers||{})[o.sampler]||{};return u.magFilter=Lt[d.magFilter]||Oe,u.minFilter=Lt[d.minFilter]||st,u.wrapS=It[d.wrapS]||Je,u.wrapT=It[d.wrapT]||Je,u.generateMipmaps=!u.isCompressedTexture&&u.minFilter!==Gt&&u.minFilter!==Oe,n.associations.set(u,{textures:t}),u}).catch(function(){return null});return this.textureCache[r]=c,c}loadImageSource(t,e){const i=this,n=this.json,s=this.options;if(this.sourceCache[t]!==void 0)return this.sourceCache[t].then(l=>l.clone());const o=n.images[t],a=self.URL||self.webkitURL;let r=o.uri||"",c=!1;if(o.bufferView!==void 0)r=i.getDependency("bufferView",o.bufferView).then(function(l){c=!0;const d=new Blob([l],{type:o.mimeType});return r=a.createObjectURL(d),r});else if(o.uri===void 0)throw new Error("THREE.GLTFLoader: Image "+t+" is missing URI and bufferView");const u=Promise.resolve(r).then(function(l){return new Promise(function(d,f){let m=d;e.isImageBitmapLoader===!0&&(m=function(x){const p=new yt(x);p.needsUpdate=!0,d(p)}),e.load(Ae.resolveURL(l,s.path),m,void 0,f)})}).then(function(l){return c===!0&&a.revokeObjectURL(r),W(l,o),l.userData.mimeType=o.mimeType||bs(o.uri),l}).catch(function(l){throw console.error("THREE.GLTFLoader: Couldn't load texture",r),l});return this.sourceCache[t]=u,u}assignTexture(t,e,i,n){const s=this;return this.getDependency("texture",i.index).then(function(o){if(!o)return null;if(i.texCoord!==void 0&&i.texCoord>0&&(o=o.clone(),o.channel=i.texCoord),s.extensions[w.KHR_TEXTURE_TRANSFORM]){const a=i.extensions!==void 0?i.extensions[w.KHR_TEXTURE_TRANSFORM]:void 0;if(a){const r=s.associations.get(o);o=s.extensions[w.KHR_TEXTURE_TRANSFORM].extendTexture(o,a),s.associations.set(o,r)}}return n!==void 0&&(o.colorSpace=n),t[e]=o,o})}assignFinalMaterial(t){const e=t.geometry;let i=t.material;const n=e.attributes.tangent===void 0,s=e.attributes.color!==void 0,o=e.attributes.normal===void 0;if(t.isPoints){const a="PointsMaterial:"+i.uuid;let r=this.cache.get(a);r||(r=new wn,ze.prototype.copy.call(r,i),r.color.copy(i.color),r.map=i.map,r.sizeAttenuation=!1,this.cache.add(a,r)),i=r}else if(t.isLine){const a="LineBasicMaterial:"+i.uuid;let r=this.cache.get(a);r||(r=new xn,ze.prototype.copy.call(r,i),r.color.copy(i.color),r.map=i.map,this.cache.add(a,r)),i=r}if(n||s||o){let a="ClonedMaterial:"+i.uuid+":";n&&(a+="derivative-tangents:"),s&&(a+="vertex-colors:"),o&&(a+="flat-shading:");let r=this.cache.get(a);r||(r=i.clone(),s&&(r.vertexColors=!0),o&&(r.flatShading=!0),n&&(r.normalScale&&(r.normalScale.y*=-1),r.clearcoatNormalScale&&(r.clearcoatNormalScale.y*=-1)),this.cache.add(a,r),this.associations.set(r,this.associations.get(i))),i=r}t.material=i}getMaterialType(){return Ut}loadMaterial(t){const e=this,i=this.json,n=this.extensions,s=i.materials[t];let o;const a={},r=s.extensions||{},c=[];if(r[w.KHR_MATERIALS_UNLIT]){const l=n[w.KHR_MATERIALS_UNLIT];o=l.getMaterialType(),c.push(l.extendParams(a,s,e))}else{const l=s.pbrMetallicRoughness||{};if(a.color=new P(1,1,1),a.opacity=1,Array.isArray(l.baseColorFactor)){const d=l.baseColorFactor;a.color.setRGB(d[0],d[1],d[2],X),a.opacity=d[3]}l.baseColorTexture!==void 0&&c.push(e.assignTexture(a,"map",l.baseColorTexture,ee)),a.metalness=l.metallicFactor!==void 0?l.metallicFactor:1,a.roughness=l.roughnessFactor!==void 0?l.roughnessFactor:1,l.metallicRoughnessTexture!==void 0&&(c.push(e.assignTexture(a,"metalnessMap",l.metallicRoughnessTexture)),c.push(e.assignTexture(a,"roughnessMap",l.metallicRoughnessTexture))),o=this._invokeOne(function(d){return d.getMaterialType&&d.getMaterialType(t)}),c.push(Promise.all(this._invokeAll(function(d){return d.extendMaterialParams&&d.extendMaterialParams(t,a)})))}s.doubleSided===!0&&(a.side=jt);const u=s.alphaMode||Ke.OPAQUE;if(u===Ke.BLEND?(a.transparent=!0,a.depthWrite=!1):(a.transparent=!1,u===Ke.MASK&&(a.alphaTest=s.alphaCutoff!==void 0?s.alphaCutoff:.5)),s.normalTexture!==void 0&&o!==ae&&(c.push(e.assignTexture(a,"normalMap",s.normalTexture)),a.normalScale=new O(1,1),s.normalTexture.scale!==void 0)){const l=s.normalTexture.scale;a.normalScale.set(l,l)}if(s.occlusionTexture!==void 0&&o!==ae&&(c.push(e.assignTexture(a,"aoMap",s.occlusionTexture)),s.occlusionTexture.strength!==void 0&&(a.aoMapIntensity=s.occlusionTexture.strength)),s.emissiveFactor!==void 0&&o!==ae){const l=s.emissiveFactor;a.emissive=new P().setRGB(l[0],l[1],l[2],X)}return s.emissiveTexture!==void 0&&o!==ae&&c.push(e.assignTexture(a,"emissiveMap",s.emissiveTexture,ee)),Promise.all(c).then(function(){const l=new o(a);return s.name&&(l.name=s.name),W(l,s),e.associations.set(l,{materials:t}),s.extensions&&$(n,l,s),l})}createUniqueName(t){const e=vn.sanitizeNodeName(t||"");return e in this.nodeNamesUsed?e+"_"+ ++this.nodeNamesUsed[e]:(this.nodeNamesUsed[e]=0,e)}loadGeometries(t){const e=this,i=this.extensions,n=this.primitiveCache;function s(a){return i[w.KHR_DRACO_MESH_COMPRESSION].decodePrimitive(a,e).then(function(r){return Nt(r,a,e)})}const o=[];for(let a=0,r=t.length;a<r;a++){const c=t[a],u=ys(c),l=n[u];if(l)o.push(l.promise);else{let d;c.extensions&&c.extensions[w.KHR_DRACO_MESH_COMPRESSION]?d=s(c):d=Nt(new Te,c,e),n[u]={primitive:c,promise:d},o.push(d)}}return Promise.all(o)}loadMesh(t){const e=this,i=this.json,n=this.extensions,s=i.meshes[t],o=s.primitives,a=[];for(let r=0,c=o.length;r<c;r++){const u=o[r].material===void 0?ws(this.cache):this.getDependency("material",o[r].material);a.push(u)}return a.push(e.loadGeometries(o)),Promise.all(a).then(function(r){const c=r.slice(0,r.length-1),u=r[r.length-1],l=[];for(let f=0,m=u.length;f<m;f++){const x=u[f],p=o[f];let g;const y=c[f];if(p.mode===k.TRIANGLES||p.mode===k.TRIANGLE_STRIP||p.mode===k.TRIANGLE_FAN||p.mode===void 0)g=s.isSkinnedMesh===!0?new yn(x,y):new Fe(x,y),g.isSkinnedMesh===!0&&g.normalizeSkinWeights(),p.mode===k.TRIANGLE_STRIP?g.geometry=Mt(g.geometry,Pt):p.mode===k.TRIANGLE_FAN&&(g.geometry=Mt(g.geometry,Qe));else if(p.mode===k.LINES)g=new zt(x,y);else if(p.mode===k.LINE_STRIP)g=new bn(x,y);else if(p.mode===k.LINE_LOOP)g=new Rn(x,y);else if(p.mode===k.POINTS)g=new Ze(x,y);else throw new Error("THREE.GLTFLoader: Primitive mode unsupported: "+p.mode);Object.keys(g.geometry.morphAttributes).length>0&&vs(g,s),g.name=e.createUniqueName(s.name||"mesh_"+t),W(g,s),p.extensions&&$(n,g,p),e.assignFinalMaterial(g),l.push(g)}for(let f=0,m=l.length;f<m;f++)e.associations.set(l[f],{meshes:t,primitives:f});if(l.length===1)return s.extensions&&$(n,l[0],s),l[0];const d=new Ne;s.extensions&&$(n,d,s),e.associations.set(d,{meshes:t});for(let f=0,m=l.length;f<m;f++)d.add(l[f]);return d})}loadCamera(t){let e;const i=this.json.cameras[t],n=i[i.type];if(!n){console.warn("THREE.GLTFLoader: Missing camera parameters.");return}return i.type==="perspective"?e=new Vt(C.radToDeg(n.yfov),n.aspectRatio||1,n.znear||1,n.zfar||2e6):i.type==="orthographic"&&(e=new En(-n.xmag,n.xmag,n.ymag,-n.ymag,n.znear,n.zfar)),i.name&&(e.name=this.createUniqueName(i.name)),W(e,i),Promise.resolve(e)}loadSkin(t){const e=this.json.skins[t],i=[];for(let n=0,s=e.joints.length;n<s;n++)i.push(this._loadNodeShallow(e.joints[n]));return e.inverseBindMatrices!==void 0?i.push(this.getDependency("accessor",e.inverseBindMatrices)):i.push(null),Promise.all(i).then(function(n){const s=n.pop(),o=n,a=[],r=[];for(let c=0,u=o.length;c<u;c++){const l=o[c];if(l){a.push(l);const d=new Ce;s!==null&&d.fromArray(s.array,c*16),r.push(d)}else console.warn('THREE.GLTFLoader: Joint "%s" could not be found.',e.joints[c])}return new Sn(a,r)})}loadAnimation(t){const e=this.json,i=this,n=e.animations[t],s=n.name?n.name:"animation_"+t,o=[],a=[],r=[],c=[],u=[];for(let l=0,d=n.channels.length;l<d;l++){const f=n.channels[l],m=n.samplers[f.sampler],x=f.target,p=x.node,g=n.parameters!==void 0?n.parameters[m.input]:m.input,y=n.parameters!==void 0?n.parameters[m.output]:m.output;x.node!==void 0&&(o.push(this.getDependency("node",p)),a.push(this.getDependency("accessor",g)),r.push(this.getDependency("accessor",y)),c.push(m),u.push(x))}return Promise.all([Promise.all(o),Promise.all(a),Promise.all(r),Promise.all(c),Promise.all(u)]).then(function(l){const d=l[0],f=l[1],m=l[2],x=l[3],p=l[4],g=[];for(let y=0,R=d.length;y<R;y++){const v=d[y],S=f[y],M=m[y],E=x[y],I=p[y];if(v===void 0)continue;v.updateMatrix&&v.updateMatrix();const N=i._createAnimationTracks(v,S,M,E,I);if(N)for(let G=0;G<N.length;G++)g.push(N[G])}return new Mn(s,void 0,g)})}createNodeMesh(t){const e=this.json,i=this,n=e.nodes[t];return n.mesh===void 0?null:i.getDependency("mesh",n.mesh).then(function(s){const o=i._getNodeRef(i.meshCache,n.mesh,s);return n.weights!==void 0&&o.traverse(function(a){if(a.isMesh)for(let r=0,c=n.weights.length;r<c;r++)a.morphTargetInfluences[r]=n.weights[r]}),o})}loadNode(t){const e=this.json,i=this,n=e.nodes[t],s=i._loadNodeShallow(t),o=[],a=n.children||[];for(let c=0,u=a.length;c<u;c++)o.push(i.getDependency("node",a[c]));const r=n.skin===void 0?Promise.resolve(null):i.getDependency("skin",n.skin);return Promise.all([s,Promise.all(o),r]).then(function(c){const u=c[0],l=c[1],d=c[2];d!==null&&u.traverse(function(f){f.isSkinnedMesh&&f.bind(d,Rs)});for(let f=0,m=l.length;f<m;f++)u.add(l[f]);return u})}_loadNodeShallow(t){const e=this.json,i=this.extensions,n=this;if(this.nodeCache[t]!==void 0)return this.nodeCache[t];const s=e.nodes[t],o=s.name?n.createUniqueName(s.name):"",a=[],r=n._invokeOne(function(c){return c.createNodeMesh&&c.createNodeMesh(t)});return r&&a.push(r),s.camera!==void 0&&a.push(n.getDependency("camera",s.camera).then(function(c){return n._getNodeRef(n.cameraCache,s.camera,c)})),n._invokeAll(function(c){return c.createNodeAttachment&&c.createNodeAttachment(t)}).forEach(function(c){a.push(c)}),this.nodeCache[t]=Promise.all(a).then(function(c){let u;if(s.isBone===!0?u=new _n:c.length>1?u=new Ne:c.length===1?u=c[0]:u=new kt,u!==c[0])for(let l=0,d=c.length;l<d;l++)u.add(c[l]);if(s.name&&(u.userData.name=s.name,u.name=o),W(u,s),s.extensions&&$(i,u,s),s.matrix!==void 0){const l=new Ce;l.fromArray(s.matrix),u.applyMatrix4(l)}else s.translation!==void 0&&u.position.fromArray(s.translation),s.rotation!==void 0&&u.quaternion.fromArray(s.rotation),s.scale!==void 0&&u.scale.fromArray(s.scale);return n.associations.has(u)||n.associations.set(u,{}),n.associations.get(u).nodes=t,u}),this.nodeCache[t]}loadScene(t){const e=this.extensions,i=this.json.scenes[t],n=this,s=new Ne;i.name&&(s.name=n.createUniqueName(i.name)),W(s,i),i.extensions&&$(e,s,i);const o=i.nodes||[],a=[];for(let r=0,c=o.length;r<c;r++)a.push(n.getDependency("node",o[r]));return Promise.all(a).then(function(r){for(let u=0,l=r.length;u<l;u++)s.add(r[u]);const c=u=>{const l=new Map;for(const[d,f]of n.associations)(d instanceof ze||d instanceof yt)&&l.set(d,f);return u.traverse(d=>{const f=n.associations.get(d);f!=null&&l.set(d,f)}),l};return n.associations=c(s),s})}_createAnimationTracks(t,e,i,n,s){const o=[],a=t.name?t.name:t.uuid,r=[];q[s.path]===q.weights?t.traverse(function(d){d.morphTargetInfluences&&r.push(d.name?d.name:d.uuid)}):r.push(a);let c;switch(q[s.path]){case q.weights:c=Rt;break;case q.rotation:c=Et;break;case q.position:case q.scale:c=bt;break;default:switch(i.itemSize){case 1:c=Rt;break;case 2:case 3:default:c=bt;break}break}const u=n.interpolation!==void 0?Ts[n.interpolation]:Kt,l=this._getArrayFromAccessor(i);for(let d=0,f=r.length;d<f;d++){const m=new c(r[d]+"."+q[s.path],e.array,l,u);n.interpolation==="CUBICSPLINE"&&this._createCubicSplineTrackInterpolant(m),o.push(m)}return o}_getArrayFromAccessor(t){let e=t.array;if(t.normalized){const i=tt(e.constructor),n=new Float32Array(e.length);for(let s=0,o=e.length;s<o;s++)n[s]=e[s]*i;e=n}return e}_createCubicSplineTrackInterpolant(t){t.createInterpolant=function(i){const n=this instanceof Et?As:qt;return new n(this.times,this.values,this.getValueSize()/3,i)},t.createInterpolant.isInterpolantFactoryMethodGLTFCubicSpline=!0}}function Ss(h,t,e){const i=t.attributes,n=new Wt;if(i.POSITION!==void 0){const a=e.json.accessors[i.POSITION],r=a.min,c=a.max;if(r!==void 0&&c!==void 0){if(n.set(new T(r[0],r[1],r[2]),new T(c[0],c[1],c[2])),a.normalized){const u=tt(ce[a.componentType]);n.min.multiplyScalar(u),n.max.multiplyScalar(u)}}else{console.warn("THREE.GLTFLoader: Missing min/max properties for accessor POSITION.");return}}else return;const s=t.targets;if(s!==void 0){const a=new T,r=new T;for(let c=0,u=s.length;c<u;c++){const l=s[c];if(l.POSITION!==void 0){const d=e.json.accessors[l.POSITION],f=d.min,m=d.max;if(f!==void 0&&m!==void 0){if(r.setX(Math.max(Math.abs(f[0]),Math.abs(m[0]))),r.setY(Math.max(Math.abs(f[1]),Math.abs(m[1]))),r.setZ(Math.max(Math.abs(f[2]),Math.abs(m[2]))),d.normalized){const x=tt(ce[d.componentType]);r.multiplyScalar(x)}a.max(r)}else console.warn("THREE.GLTFLoader: Missing min/max properties for accessor POSITION.")}}n.expandByVector(a)}h.boundingBox=n;const o=new $e;n.getCenter(o.center),o.radius=n.min.distanceTo(n.max)/2,h.boundingSphere=o}function Nt(h,t,e){const i=t.attributes,n=[];function s(o,a){return e.getDependency("accessor",o).then(function(r){h.setAttribute(a,r)})}for(const o in i){const a=et[o]||o.toLowerCase();a in h.attributes||n.push(s(i[o],a))}if(t.indices!==void 0&&!h.index){const o=e.getDependency("accessor",t.indices).then(function(a){h.setIndex(a)});n.push(o)}return St.workingColorSpace!==X&&"COLOR_0"in i&&console.warn(`THREE.GLTFLoader: Converting vertex colors from "srgb-linear" to "${St.workingColorSpace}" not supported.`),W(h,t),Ss(h,t,e),Promise.all(n).then(function(){return t.targets!==void 0?xs(h,t.targets,e):h})}function Yt(h,t){const{count:e,size:i=10}=t,n=h.index?h.toNonIndexed():h.clone();n.computeVertexNormals();const s=n.getAttribute("position"),o=n.getAttribute("normal"),a=Math.floor(s.count/3),r=new Float64Array(a),c=new T,u=new T,l=new T,d=new T,f=new T,m=new T;let x=0;for(let b=0;b<a;b+=1)c.fromBufferAttribute(s,b*3),u.fromBufferAttribute(s,b*3+1),l.fromBufferAttribute(s,b*3+2),d.subVectors(u,c),f.subVectors(l,c),x+=m.crossVectors(d,f).length()*.5,r[b]=x;const p=new Float32Array(e*3),g=new Float32Array(e*3),y=new T,R=new T,v=new T,S=new T,M=new T,E=new Wt;for(let b=0;b<e;b+=1){const we=Math.random()*x;let D=0,te=a-1;for(;D<te;){const B=D+te>>1;(r[B]??0)<we?D=B+1:te=B}c.fromBufferAttribute(s,D*3),u.fromBufferAttribute(s,D*3+1),l.fromBufferAttribute(s,D*3+2),y.fromBufferAttribute(o,D*3),R.fromBufferAttribute(o,D*3+1),v.fromBufferAttribute(o,D*3+2);const le=Math.sqrt(Math.random()),U=Math.random(),ne=1-le,se=le*(1-U),Y=le*U;S.set(0,0,0).addScaledVector(c,ne).addScaledVector(u,se).addScaledVector(l,Y),M.set(0,0,0).addScaledVector(y,ne).addScaledVector(R,se).addScaledVector(v,Y).normalize(),p[b*3]=S.x,p[b*3+1]=S.y,p[b*3+2]=S.z,g[b*3]=M.x,g[b*3+1]=M.y,g[b*3+2]=M.z,E.expandByPoint(S)}n.dispose();const I=E.getCenter(new T),N=E.getSize(new T),G=i/Math.max(N.x,N.y,N.z,1e-6);for(let b=0;b<e;b+=1)p[b*3]=(p[b*3]-I.x)*G,p[b*3+1]=(p[b*3+1]-I.y)*G,p[b*3+2]=(p[b*3+2]-I.z)*G;return{positions:p,normals:g,count:e}}async function Ms(h,t){try{const i=await new Wn().loadAsync(h),n=[];if(i.scene.updateMatrixWorld(!0),i.scene.traverse(a=>{if(!(a instanceof Fe))return;const r=a.geometry.clone();r.applyMatrix4(a.matrixWorld);for(const c of Object.keys(r.attributes))c!=="position"&&c!=="normal"&&r.deleteAttribute(c);n.push(r.index?r.toNonIndexed():r)}),n.length===0)return null;const s=_s(n),o=Yt(s,t);s.dispose();for(const a of n)a.dispose();return o}catch{return null}}function _s(h){let t=0;for(const o of h)t+=o.getAttribute("position").count;const e=new Float32Array(t*3),i=new Float32Array(t*3);let n=0;for(const o of h){const a=o.getAttribute("position"),r=o.getAttribute("normal");for(let c=0;c<a.count;c+=1)e[(n+c)*3]=a.getX(c),e[(n+c)*3+1]=a.getY(c),e[(n+c)*3+2]=a.getZ(c),r&&(i[(n+c)*3]=r.getX(c),i[(n+c)*3+1]=r.getY(c),i[(n+c)*3+2]=r.getZ(c));n+=a.count}const s=new Te;return s.setAttribute("position",new F(e,3)),s.setAttribute("normal",new F(i,3)),s}function Ls(){const h=[new O(0,.62),new O(.18,.58),new O(.3,.44),new O(.34,.4),new O(1.45,-.34),new O(1.6,-.4),new O(1.74,-.22),new O(1.88,-.38),new O(2,-.42),new O(2.06,-.42)],t=new On(h,96);return t.rotateX(-Math.PI/2),t.computeVertexNormals(),t}const nt=1180/720,ge=6,Xe=[8,11,10,7.2,9.4,5.8],qe=[8,11,4.7,5.9,10.4,6.5],Is=.78,Ns=.92,Cs=.17,Os=-.05;function Ct(h){return Xe[Math.max(0,Math.min(Xe.length-1,h))]??Xe[0]}function Ot(h){const t=Math.max(0,Math.min(qe.length-1,h));return qe[t]??qe[0]}const Fs=.8,Ps=.34,Ds=.62,Hs=.88,ks=2.4,Ye=120;function Bs(){const h=document.querySelector('meta[name="gnarl-model"]')?.getAttribute("content")?.trim();return h||null}const Ft=`
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
`,Gs=`
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
    float H = ${(13/nt).toFixed(4)};

    float across = step(0.5, fract(c * 2.0));
    float lane = floor(c * 30.0) / 29.0;

    vec2 p = across > 0.5
      ? vec2((t - 0.5) * W, (lane - 0.5) * H)
      : vec2((lane - 0.5) * W, (t - 0.5) * H);

    return vec3(p, (s.z - 0.5) * 0.25);
  }

  /*  THE INSTRUMENT, SHUT. The section says the thing is BOUGHT - one
      price, yours, done - so the subject is the instrument as an object
      rather than as a process: its own rectangle folded back at every edge
      into a closed case.

      It is built on the panel's geometry deliberately, because the next
      formation IS the panel. The case does not morph into an unrelated
      shape at the end of the journey, it OPENS: the walls swing flat and
      the face becomes the interface. That is the one transition on this
      page where the two forms are the same object in two states, and it is
      the reason this sits here rather than the faceted solid it replaces -
      a gem was a thing to look at, this is the thing being sold.

      W is the panel's width divided by the lip, so the flat FACE comes out
      at exactly the panel's 13 across and the walls are the extra. Sizing
      the whole sheet at 13 instead would have folded a third of the
      instrument away into the sides and left a case visibly smaller than
      the screen it turns into. */
  vec3 formShell(float c, float t, vec3 s) {
    /*  How much of the sheet stays flat. The rest is wall. */
    const float LIP = 0.78;
    const float DEPTH = 3.1;

    float W = ${(13/.78).toFixed(4)};
    float H = ${(13/nt/.78).toFixed(4)};

    float across = step(0.5, fract(c * 2.0));
    float lane = floor(c * 30.0) / 29.0;

    vec2 p = across > 0.5
      ? vec2((t - 0.5) * W, (lane - 0.5) * H)
      : vec2((lane - 0.5) * W, (t - 0.5) * H);

    /*  A SQUARE distance, not a radial one. The fold has to run parallel to
        the edges or the case comes out as a bowl - max() of the two axes is
        what makes the crease a rectangle and the corners meet properly. */
    float m = max(abs(p.x) / (W * 0.5), abs(p.y) / (H * 0.5));

    float k = clamp((m - LIP) / (1.0 - LIP), 0.0, 1.0);

    /*  Squared, so the sheet leaves the face TANGENTIALLY. A linear bend
        puts a hard kink at the lip and the case reads as folded paper;
        easing in gives it the drawn radius a moulded corner has. */
    float bend = k * k;

    /*  Past the lip the ruling stops growing outward and is carried back
        instead, tapering slightly as it goes - a wall that is exactly
        perpendicular reads as an open box, and one that leans in reads as a
        lid that closes onto something. */
    float pull = mix(1.0, (LIP / max(m, 0.0001)) * 0.93, bend);

    return vec3(p * pull, -bend * DEPTH + (s.z - 0.5) * 0.18);
  }

  vec3 formation(int id, float c, float t, vec3 s) {
    if (id <= 0) return formOrbits(c, t, s);
    if (id == 1) return formWave(c, t, s);
    if (id == 2) return formHelix(c, t, s);
    if (id == 3) return formDriver(c, t, s);
    if (id == 4) return formShell(c, t, s);
    return formPanel(c, t, s);
  }
`;async function Ws(h){const t=window.matchMedia("(prefers-reduced-motion: reduce)").matches;let e=window.innerWidth<860;const i=new Fn({canvas:h,antialias:!e,powerPreference:"high-performance"});i.setPixelRatio(Math.min(window.devicePixelRatio,e?1.6:2)),i.toneMapping=Pn,i.toneMappingExposure=1;const n=new Dn;n.background=new P(197386);const s=new Vt(52,1,.1,900),o=[],a=new Hn([new T(0,0,0),new T(22,8,-60),new T(-14,-6,-130),new T(18,10,-205),new T(-8,2,-280),new T(6,-4,-350)],!1,"catmullrom",.5),r=e?3e3:12e3,c=new Te,u=new Float32Array(r*3),l=new Float32Array(r*2),d=new T;for(let A=0;A<r;A+=1){a.getPointAt(Math.random(),d);const _=45+Math.random()*130,K=Math.random()*Math.PI*2,L=Math.acos(1-2*Math.random());u[A*3]=d.x+_*Math.sin(L)*Math.cos(K),u[A*3+1]=d.y+_*Math.cos(L),u[A*3+2]=d.z+_*Math.sin(L)*Math.sin(K),l[A*2]=Math.random(),l[A*2+1]=Math.random()}c.setAttribute("position",new F(u,3)),c.setAttribute("aSeed",new F(l,2)),o.push(c);const f={uTime:{value:0},uPixelRatio:{value:i.getPixelRatio()}},m=new Le({uniforms:f,transparent:!0,depthWrite:!1,blending:Ie,vertexShader:`
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
    `});o.push(m);const x=new Ze(c,m);x.frustumCulled=!1,n.add(x);const p=new Ne;n.add(p);const g=e?34:56,y=Ye*g*2,R=new Te,v=new Float32Array(y),S=new Float32Array(y),M=new Float32Array(y*3);let E=0;for(let A=0;A<Ye;A+=1){const _=A/(Ye-1),K=Math.random(),L=Math.random(),z=Math.random();for(let H=0;H<g;H+=1)for(const Me of[H/g,(H+1)/g])v[E]=_,S[E]=Me,M[E*3]=K,M[E*3+1]=L,M[E*3+2]=z,E+=1}R.setAttribute("position",new F(new Float32Array(y*3),3)),R.setAttribute("aCurve",new F(v,1)),R.setAttribute("aT",new F(S,1)),R.setAttribute("aSeed",new F(M,3)),R.boundingSphere=new $e(new T,40),o.push(R);const I={uForm:{value:0},uTime:{value:0},uFade:{value:1},uAssemble:{value:0},uAccent:{value:new P(8251135)},uAccent2:{value:new P(12160255)}},N=new Le({uniforms:I,transparent:!0,depthWrite:!1,blending:Ie,vertexShader:`
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

      ${Gs}

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
        float f = clamp(uForm, 0.0, ${(ge-1).toFixed(1)});
        int   a = int(floor(f));
        int   b = int(min(floor(f) + 1.0, ${(ge-1).toFixed(1)}));
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
    `});o.push(N);const G=new zt(R,N);G.frustumCulled=!1,p.add(G);const b=new kn({map:Us(),color:12577279,transparent:!0,blending:Ie,depthWrite:!1});o.push(b);const we=new Bn(b);p.add(we);const D=e?22e3:55e3,te=Bs(),le=te?await Ms(te,{count:D,size:11}):null,U=Ls();U.computeBoundingBox();const ne=U.boundingBox.getSize(new T),se=11/Math.max(ne.x,ne.y,ne.z);U.scale(se,se,se),U.center(),o.push(U);const Y=le??Yt(U,{count:D,size:11}),B={uThreshold:{value:-.2},uEdge:{value:.07},uFade:{value:0},uBody:{value:new P(2761040)},uRim:{value:new P(8251135)},uHot:{value:new P(16751164)}},ot=new Le({uniforms:B,side:jt,transparent:!0,vertexShader:`
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

      ${Ft}

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
    `});o.push(ot);const xe=new Fe(U,ot);p.add(xe);const oe=new Te;oe.setAttribute("position",new F(Y.positions,3)),oe.setAttribute("aNormal",new F(Y.normals,3));const it=new Float32Array(Y.count*3);for(let A=0;A<Y.count*3;A+=1)it[A]=Math.random();oe.setAttribute("aSeed",new F(it,3)),oe.boundingSphere=new $e(new T,60),o.push(oe);const ve={uThreshold:B.uThreshold,uFade:B.uFade,uTime:{value:0},uPixelRatio:{value:i.getPixelRatio()},uHot:B.uHot,uCool:{value:new P(8251135)}},rt=new Le({uniforms:ve,transparent:!0,depthWrite:!1,blending:Ie,vertexShader:`
      precision highp float;

      attribute vec3 aNormal;
      attribute vec3 aSeed;

      uniform float uThreshold;
      uniform float uTime;
      uniform float uPixelRatio;

      varying float vAge;

      ${Ft}

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
    `});o.push(rt);const ye=new Ze(oe,rt);ye.frustumCulled=!1,p.add(ye);const Q=new Bt().load("./ui-osc.webp");Q.colorSpace=ee,Q.anisotropy=i.capabilities.getMaxAnisotropy(),Q.minFilter=st,Q.magFilter=Oe,Q.generateMipmaps=!0,o.push(Q);const Pe=new ae({map:Q,transparent:!0,opacity:0,toneMapped:!1,depthWrite:!1});o.push(Pe);const ue=new Fe(new Gn(13,13/nt),Pe);o.push(ue.geometry),p.add(ue);const ie=new jn(i);ie.addPass(new zn(n,s));const at=new Vn(new O(1,1),e?.6:.82,.66,.28);ie.addPass(at),ie.addPass(new Kn);const De=new O(.5,.5),be=new O(.5,.5);let Re=0,he=0;const j=new T,ct=new T,lt=new T,He=new T;let ut=!1,Ee=1;const ht=45;let ke=0,Be=0,Ge=0,Ue=0;const de=new T,J=new T,fe=new T,Z=new T,dt=new T;a.getPointAt(0,s.position),s.position.z+=26;let ft=1,pt=0,Se=0;function mt(){const A=h.clientWidth,_=h.clientHeight;e=window.innerWidth<860,i.setSize(A,_,!1),ie.setSize(A,_);const L=Math.min(e?.5:1,(e?360:1280)/Math.max(1,A));at.setSize(Math.round(A*L),Math.round(_*L)),f.uPixelRatio.value=i.getPixelRatio(),ve.uPixelRatio.value=i.getPixelRatio(),s.aspect=A/_,s.fov=e?66:52,s.updateProjectionMatrix();const z=A!==pt,H=Math.abs(_-Se);(z||H>Se*.25||Se===0)&&(pt=A,Se=_,ft=A/Math.max(1,_))}return{setLateral:A=>{Ue=Math.max(0,Math.min(1,A))},setScroll(A){Re=A},setPointer(A,_){De.set(A,_)},isSettled(){return ut},setEntry(A){Ee=Math.max(0,Math.min(1,A))},setAssemble(A){I.uAssemble.value=Math.max(0,Math.min(1,A))},resize:mt,render(A,_){I.uTime.value=A,f.uTime.value=A,ve.uTime.value=A;const K=re=>1-Math.exp(-re*Math.min(_,.1));t?(he=Re,be.copy(De)):(he+=(Re-he)*K(4),be.lerp(De,K(2.5)));const L=Math.min(1,Math.max(0,he));a.getPointAt(L,j),a.getPointAt(Math.max(0,L-.055),ct),a.getPointAt(Math.min(1,L+.02),lt),p.position.copy(j),fe.subVectors(ct,lt).normalize().multiplyScalar(.86),fe.y+=.21+(be.y-.5)*-.16,fe.x+=(be.x-.5)*.24,fe.normalize();const z=L*(ge-1),H=Math.min(ge-1,Math.floor(z)),Me=Math.min(ge-1,H+1),Qt=C.lerp(Ct(H),Ct(Me),z-H),Jt=C.lerp(Ot(H),Ot(Me),z-H),je=C.smoothstep(z,4.2,5),Zt=e?C.lerp(Ps,Ds,je):C.lerp(Fs,Hs,je),gt=Math.tan(s.fov*Math.PI/360),$t=Qt/(Zt*gt),en=C.lerp(Is,Ns,je),tn=Jt/(en*gt*Math.max(.1,ft)),nn=Math.max($t,tn)*(1+Ee*ks);if(He.copy(j).addScaledVector(fe,nn),s.position.lerp(He,t?1:K(2.2)),ut=Math.abs(Re-he)<5e-4&&s.position.distanceTo(He)<.05&&Ee<.001&&I.uAssemble.value>.999,de.copy(j),e){const pe=2*s.position.distanceTo(j)*Math.tan(s.fov*Math.PI/360);J.subVectors(j,s.position).normalize(),Z.crossVectors(J,s.up).normalize(),dt.crossVectors(Z,J).normalize();const _e=C.lerp(Cs,Os,C.smoothstep(z,4,5));de.addScaledVector(dt,-pe*_e)}else{const pe=2*s.position.distanceTo(j)*Math.tan(s.fov*Math.PI/360)*s.aspect;J.subVectors(j,s.position).normalize(),Z.crossVectors(J,s.up).normalize(),de.addScaledVector(Z,-pe*.2)}if(Ue>1e-4){const pe=2*s.position.distanceTo(j)*Math.tan(s.fov*Math.PI/360)*s.aspect;J.subVectors(j,s.position).normalize(),Z.crossVectors(J,s.up).normalize();const _e=pe*1.25*Ue;s.position.addScaledVector(Z,_e),de.addScaledVector(Z,_e)}s.lookAt(de),I.uForm.value=z;const At=C.smoothstep(L,.58,.66),Tt=C.smoothstep(L,.7,.78);B.uFade.value=At*(1-Tt);const wt=B.uFade.value>.001;xe.visible=wt,ye.visible=wt,B.uThreshold.value=C.lerp(-.05,1.5,C.smoothstep(L,.62,.8)),I.uFade.value=1-At*(1-Tt)*.75;const xt=C.smoothstep(L,.9,.995);Pe.opacity=xt*.95,ue.visible=xt>.001;const vt=1-C.smoothstep(L,0,.22);b.opacity=.1+vt*.52,we.scale.setScalar(3+vt*2.8),ue.quaternion.copy(s.quaternion),ue.position.set(0,0,0),xe.rotation.y=A*.12,ye.rotation.y=xe.rotation.y;const sn=Ee<.001&&I.uAssemble.value>.999;if(ke<2&&sn&&(Ge+=1,_>.045&&(Be+=1),Ge>=ht)){if(Be>ht/3){ke+=1;const re=ke===1?1.15:.85;i.setPixelRatio(Math.min(window.devicePixelRatio,re)),f.uPixelRatio.value=i.getPixelRatio(),ve.uPixelRatio.value=i.getPixelRatio(),mt()}Be=0,Ge=0}ie.render()},dispose(){for(const A of o)A.dispose();ie.dispose(),i.dispose()}}}function Us(){const t=document.createElement("canvas");t.width=128,t.height=128;const e=t.getContext("2d");if(e){const n=e.createRadialGradient(64,64,0,64,64,64);n.addColorStop(0,"rgba(255,255,255,1)"),n.addColorStop(.18,"rgba(255,255,255,0.65)"),n.addColorStop(.45,"rgba(255,255,255,0.16)"),n.addColorStop(1,"rgba(255,255,255,0)"),e.fillStyle=n,e.fillRect(0,0,128,128)}const i=new Un(t);return i.colorSpace=ee,i}export{Ws as createJourney};
