#pragma once
#include <string>

namespace lf {
namespace webui_detail {

inline const char* const kHtml1 = R"LF(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0f766e">
<title>校园失物招领</title>
<style>
:root{--pri:#0f766e;--pri2:#14b8a6;--priD:#0b5f58;--bg:#eef4f3;--card:#fff;--txt:#20302f;--muted:#6b7b84;--line:#dfe8e6;--ok:#0e9f6e;--warn:#d97706;--bad:#dc2626;--info:#2563eb;--r:14px;--sh:0 2px 12px rgba(16,64,58,.08);--max:1100px}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--txt);font:15px/1.6 system-ui,-apple-system,"Segoe UI","Microsoft YaHei",sans-serif;padding-bottom:52px}
a{color:inherit;text-decoration:none}
h1,h2,h3{margin:0}
.wrap{max-width:var(--max);margin:0 auto;padding:0 16px}
.muted{color:var(--muted);font-size:13px}
.topbar{position:sticky;top:0;z-index:20;background:linear-gradient(120deg,#0f766e,#14b8a6);color:#fff;box-shadow:0 2px 10px rgba(10,80,70,.22)}
.tb{display:flex;align-items:center;gap:16px;height:58px}
.brand{display:flex;align-items:center;gap:7px;font-weight:700;font-size:17px;white-space:nowrap}
.brand .em{font-size:20px}
.nav{display:flex;gap:2px;flex:1;min-width:0;overflow-x:auto;scrollbar-width:none}
.nav a{padding:7px 12px;border-radius:9px;font-size:14px;opacity:.9;white-space:nowrap}
.nav a:hover,.nav a.on{background:rgba(255,255,255,.2);opacity:1}
.nav a.on{font-weight:600}
.tbr{display:flex;align-items:center;gap:10px}
.bell{position:relative;font-size:18px;line-height:1}
.bell b{position:absolute;top:-6px;right:-11px;background:#ef4444;color:#fff;font-size:10px;line-height:1;padding:3px 4px;border-radius:9px;font-weight:700}
.bell b.zero{display:none}
.who{background:rgba(255,255,255,.18);padding:5px 11px;border-radius:20px;font-size:13px;max-width:110px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
main{padding-top:20px;min-height:62vh}
.btn{display:inline-flex;align-items:center;justify-content:center;gap:5px;border:1px solid var(--line);background:#fff;color:var(--txt);padding:8px 14px;border-radius:10px;cursor:pointer;font-size:14px;font-family:inherit;transition:.15s;white-space:nowrap}
.btn:hover{border-color:var(--pri2);color:var(--pri);background:#f1fbf9}
.btn:focus-visible{outline:2px solid var(--pri2);outline-offset:2px}
.btn.primary{background:var(--pri);border-color:var(--pri);color:#fff}
.btn.primary:hover{background:var(--priD);border-color:var(--priD);color:#fff}
.btn.danger{color:var(--bad);border-color:#f2c9c9}
.btn.danger:hover{background:#fef2f2;border-color:var(--bad);color:var(--bad)}
.btn.sm{padding:5px 10px;font-size:13px;border-radius:8px}
.btn.block{width:100%;padding:11px}
.btn:disabled{opacity:.5;cursor:not-allowed}
.input{width:100%;border:1px solid var(--line);background:#fff;border-radius:10px;padding:9px 12px;font-size:14px;font-family:inherit;color:inherit;transition:.15s}
.input:hover{border-color:#bcd6d2}
.input:focus{outline:none;border-color:var(--pri2);box-shadow:0 0 0 3px rgba(20,184,166,.16)}
textarea.input{resize:vertical;line-height:1.6}
.panel{background:var(--card);border-radius:var(--r);box-shadow:var(--sh);padding:16px;margin-bottom:16px}
.hero{display:flex;align-items:center;justify-content:space-between;gap:14px;margin-bottom:16px;flex-wrap:wrap}
.hero h2{font-size:21px}
.sub{color:var(--muted);font-size:13px;margin:4px 0 0}
.tabs{display:flex;gap:6px;flex-wrap:wrap}
.tabs.center{justify-content:center;margin:14px 0}
.tab{border:1px solid var(--line);background:#fff;border-radius:20px;padding:6px 16px;font-size:14px;cursor:pointer;color:var(--muted);font-family:inherit;transition:.15s}
.tab:hover{color:var(--pri);border-color:var(--pri2)}
.tab.on{background:var(--pri);border-color:var(--pri);color:#fff;font-weight:600}
.filters{display:flex;gap:10px;flex-wrap:wrap;margin-top:12px}
.filters .fl{width:auto;min-width:132px;flex:0 1 auto}
.searchbox{display:flex;gap:8px;flex:1;min-width:220px}
.listmeta{display:flex;align-items:center;justify-content:space-between;margin:16px 2px 10px;font-size:13px;color:var(--muted)}
.dotmark{background:#fef3c7;color:#92400e;border-radius:20px;padding:2px 10px;font-size:12px}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(235px,1fr));gap:14px}
.card{background:var(--card);border-radius:var(--r);box-shadow:var(--sh);overflow:hidden;display:block;transition:transform .16s,box-shadow .16s}
.card:hover{transform:translateY(-3px);box-shadow:0 8px 20px rgba(16,64,58,.14)}
.thumb{position:relative;aspect-ratio:4/3;background:linear-gradient(135deg,#d5efeb,#a3d7cf);display:flex;align-items:center;justify-content:center;overflow:hidden}
.thumb img{width:100%;height:100%;object-fit:cover;display:block}
.ph{font-size:40px;opacity:.9}
.thumb .badge{position:absolute;left:10px;top:10px}
.cbody{padding:12px 13px 14px}
.ctitle{font-size:15px;margin:8px 0 6px;line-height:1.4;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden}
.cli{font-size:12.5px;color:var(--muted);margin:2px 0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.cfoot{display:flex;justify-content:space-between;margin-top:10px;padding-top:9px;border-top:1px dashed var(--line);font-size:12px;color:var(--muted)}
.badge{display:inline-block;font-size:12px;line-height:1.5;padding:1px 8px;border-radius:20px;font-weight:600;white-space:nowrap;background:#eef2f1;color:#4b5b58}
.ty0{background:#e0f2fe;color:#075985}
.ty1{background:#ccfbf1;color:#115e59}
.st0{background:#dbeafe;color:#1d4ed8}
.st1{background:#fef3c7;color:#92400e}
.st2{background:#cffafe;color:#155e75}
.st3{background:#d1fae5;color:#065f46}
.st4{background:#e5e7eb;color:#4b5563}
.st5{background:#fee2e2;color:#991b1b}
.crow{margin-top:2px}
.chips{display:flex;gap:6px;flex-wrap:wrap;align-items:center}
.pager{display:flex;align-items:center;justify-content:center;gap:6px;flex-wrap:wrap;margin:20px 0 6px;font-size:13px;color:var(--muted)}
.pager button{min-width:34px;border:1px solid var(--line);background:#fff;border-radius:9px;padding:6px 10px;cursor:pointer;font-family:inherit;font-size:13px}
.pager button:hover:not(:disabled){border-color:var(--pri2);color:var(--pri)}
.pager button.on{background:var(--pri);border-color:var(--pri);color:#fff}
.pager button:disabled{opacity:.45;cursor:not-allowed}
.pages{display:flex;gap:6px}
.loading{display:flex;align-items:center;justify-content:center;gap:10px;padding:44px 10px;color:var(--muted);font-size:14px}
.spin{width:18px;height:18px;border:2px solid #cfe4e1;border-top-color:var(--pri);border-radius:50%;animation:sp .8s linear infinite}
@keyframes sp{to{transform:rotate(360deg)}}
.empty{text-align:center;padding:52px 16px;color:var(--muted)}
.eicon{font-size:44px;margin-bottom:6px}
.empty h3{color:var(--txt);font-size:16px;margin-bottom:4px}
.empty .btn{margin-top:14px}
.back{display:inline-block;margin-bottom:14px;color:var(--pri);font-size:14px}
.back:hover{text-decoration:underline}
.detail{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1.45fr);gap:18px;align-items:start}
.dgallery{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:8px}
.gimg{padding:0;border:0;background:transparent;cursor:zoom-in;border-radius:10px;overflow:hidden;box-shadow:var(--sh)}
.gimg img{width:100%;height:150px;object-fit:cover;display:block;transition:.2s}
.gimg:hover img{transform:scale(1.05)}
.gph{grid-column:1/-1;aspect-ratio:4/3;border-radius:12px;background:linear-gradient(135deg,#d5efeb,#a3d7cf);display:flex;align-items:center;justify-content:center;color:#276b64;font-size:16px}
.dmain{background:var(--card);border-radius:var(--r);box-shadow:var(--sh);padding:20px}
.dtitle{font-size:22px;margin:10px 0 4px;line-height:1.35}
.dsub{color:var(--muted);font-size:13px;margin:0 0 14px}
.info{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px;margin:0;padding:12px;background:#f6faf9;border-radius:12px}
.info dt{color:var(--muted);font-size:12px}
.info dd{margin:2px 0 0;font-size:14px}
)LF";
inline const char* const kHtml2 = R"LF(.block{margin-top:18px;padding-top:16px;border-top:1px solid var(--line)}
.block h3{font-size:15px;margin-bottom:8px}
.desc{white-space:pre-wrap;word-break:break-word;margin:0;font-size:14.5px}
.qbox{background:#fffbeb;border-radius:12px;padding:12px 14px;border:1px solid #fde68a}
.qbox h3{color:#92400e}
.dactions{display:flex;gap:10px;flex-wrap:wrap;margin-top:18px;align-items:center}
.notice{background:#f0fdfa;border:1px dashed #99e1d5;color:#115e59;border-radius:10px;padding:9px 12px;font-size:13.5px;width:100%}
.flabel{display:block;font-size:13.5px;color:#3d4f4c;margin:12px 0 5px;font-weight:600}
.req{color:var(--bad)}
.frow{display:flex;gap:12px;flex-wrap:wrap}
.frow .flabel{flex:1;min-width:170px}
.fbtns{display:flex;gap:10px;margin-top:16px;flex-wrap:wrap}
.editform,.claimform,.pform{margin-top:16px}
.code{letter-spacing:3px;font-family:ui-monospace,Consolas,monospace;font-weight:700}
.pickup{background:#ecfdf5;border:1px solid #a7f3d0;border-radius:9px;padding:6px 10px;font-size:13.5px;color:#065f46;margin:8px 0 0;display:inline-block}
.pickup b{font-size:19px;letter-spacing:3px;color:#047857;margin-left:4px}
.vform{display:flex;gap:8px;margin-top:12px;flex-wrap:wrap}
.vform .input{flex:1;min-width:150px}
.vform .code{max-width:190px}
.claimrow{border:1px solid var(--line);border-radius:12px;padding:12px;margin-bottom:10px;background:#fcfefe}
.crhead{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:6px}
.crline{margin:3px 0;font-size:13.5px}
.crline span{color:var(--muted)}
.crline b.code{font-size:18px;color:var(--pri)}
.crbtns{display:flex;gap:8px;margin-top:8px}
.sechead{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:10px;flex-wrap:wrap}
.authwrap{display:flex;justify-content:center;padding:10px 0 30px}
.authcard{background:var(--card);border-radius:18px;box-shadow:0 10px 30px rgba(16,64,58,.12);padding:26px;width:100%;max-width:420px}
.authcard.wide{max-width:620px}
.authhead{text-align:center;margin-bottom:8px}
.alogo{font-size:38px;line-height:1.1}
.authhead h1{font-size:20px;margin-top:4px}
.authhead .muted{margin:4px 0 0}
.hint{text-align:center;color:var(--muted);font-size:12.5px;margin:14px 0 0}
.pagehead{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:16px;flex-wrap:wrap}
.pagehead h2{font-size:20px}
.rows{display:flex;flex-direction:column;gap:10px}
.rowitem{display:flex;gap:12px;justify-content:space-between;align-items:center;background:var(--card);border-radius:12px;box-shadow:var(--sh);padding:13px 15px;flex-wrap:wrap}
.rimain{min-width:0;flex:1}
.rititle{font-weight:600;font-size:15px}
.rititle:hover{color:var(--pri)}
.rimain .chips{margin:5px 0}
.rimain .muted{margin:2px 0 0}
.ribtns{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.notifs{display:flex;flex-direction:column;gap:9px}
.notif{display:flex;gap:11px;text-align:left;width:100%;background:var(--card);border:1px solid transparent;border-radius:12px;box-shadow:var(--sh);padding:12px 14px;cursor:pointer;font-family:inherit;transition:.15s}
.notif:hover{border-color:var(--pri2)}
.notif.unread{background:#f0fdfa;border-color:#b5ece2}
.nicon{font-size:18px;line-height:1.4}
.nbody{display:flex;flex-direction:column;gap:2px;min-width:0}
.ntitle{font-weight:600;font-size:14.5px}
.ntext{font-size:13px;color:#41524f;word-break:break-word}
.ntime{font-size:12px;color:var(--muted)}
.stats{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:12px;margin-bottom:16px}
.stat{background:var(--card);border-radius:var(--r);box-shadow:var(--sh);padding:16px;text-align:center}
.stat b{display:block;font-size:26px;color:var(--pri);line-height:1.2}
.stat span{font-size:12.5px;color:var(--muted)}
.invs{display:flex;flex-direction:column;gap:8px}
.inv{display:flex;gap:12px;align-items:center;flex-wrap:wrap;border:1px dashed var(--line);border-radius:10px;padding:9px 12px}
.inv code{background:#ecfdf5;color:#065f46;padding:3px 9px;border-radius:7px;font-weight:700}
.logs{max-height:330px;overflow:auto;display:flex;flex-direction:column;gap:6px}
.log{display:flex;gap:10px;flex-wrap:wrap;align-items:baseline;font-size:13px;border-bottom:1px dashed var(--line);padding-bottom:6px}
.lact{font-weight:600;color:var(--pri)}
.ldet{color:#41524f;flex:1;min-width:120px;word-break:break-word}
.lm{color:var(--muted);font-size:12px}
.profile{display:flex;gap:18px;align-items:center;background:var(--card);border-radius:var(--r);box-shadow:var(--sh);padding:20px;margin-bottom:16px;flex-wrap:wrap}
.avatar{width:64px;height:64px;border-radius:50%;background:linear-gradient(135deg,#0f766e,#14b8a6);color:#fff;font-size:26px;font-weight:700;display:flex;align-items:center;justify-content:center}
.pinfo{flex:1;min-width:220px}
.pinfo h3{font-size:18px;margin-bottom:8px;display:flex;gap:8px;align-items:center}
.pinfo .info{background:transparent;padding:0}
.previews{display:flex;gap:10px;flex-wrap:wrap;margin-top:10px}
.pv{width:104px;border-radius:10px;overflow:hidden;background:#fff;box-shadow:var(--sh)}
.pv img{width:100%;height:78px;object-fit:cover;display:block}
.pv span{display:block;font-size:11px;color:var(--muted);padding:3px 5px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.toast{position:fixed;top:14px;left:50%;transform:translate(-50%,-24px);background:#1f2d2b;color:#fff;padding:11px 20px;border-radius:24px;font-size:14px;box-shadow:0 8px 24px rgba(0,0,0,.22);opacity:0;pointer-events:none;transition:.22s;z-index:60;max-width:88vw}
.toast.show{opacity:1;transform:translate(-50%,0)}
.toast.err,.toast.warn{background:#b91c1c}
.toast.ok{background:#047857}
.toast.info{background:#1d4ed8}
.modal{position:fixed;inset:0;background:rgba(15,32,30,.62);display:flex;align-items:center;justify-content:center;padding:18px;z-index:50}
.modal.hide{display:none}
.modal-box{background:#fff;border-radius:16px;padding:20px;width:100%;max-width:440px;max-height:88vh;overflow:auto;box-shadow:0 20px 60px rgba(0,0,0,.3)}
.modal-box h3{font-size:18px;margin-bottom:6px}
.zoombar{text-align:right;margin-bottom:8px}
.zoomimg{max-width:100%;border-radius:10px;display:block;margin:0 auto}
.tabbar{display:none}
@media (max-width:720px){
.nav{display:none}
.tb{height:52px;gap:10px}
.brand{font-size:15.5px}
.detail{grid-template-columns:1fr}
.gimg img{height:130px}
.grid{grid-template-columns:1fr}
.filters .fl{flex:1;min-width:calc(50% - 6px)}
.searchbox{min-width:100%}
.hero h2{font-size:18px}
.dmain{padding:16px}
.dtitle{font-size:19px}
body{padding-bottom:78px}
.tabbar{display:flex;position:fixed;left:0;right:0;bottom:0;background:#fff;border-top:1px solid var(--line);box-shadow:0 -2px 12px rgba(16,64,58,.08);z-index:25;padding:6px 0 8px}
.tabbar a{flex:1;text-align:center;font-size:11px;color:var(--muted);padding:3px 0;line-height:1.3}
.tabbar a span{display:block}
.rowitem{padding:12px}
.ribtns{width:100%;justify-content:flex-end}
}
</style>
</head>
<body>
<header class="topbar"><div class="wrap tb">
<a class="brand" href="#/items"><span class="em">🧭</span><span>校园失物招领</span></a>
<nav class="nav" id="nav"></nav>
<div class="tbr" id="tbr"></div>
</div></header>
<main class="wrap" id="view"><div class="loading"><span class="spin"></span>加载中…</div></main>
<nav class="tabbar" id="tabbar"></nav>
<div class="toast" id="toast"></div>
<div class="modal hide" id="modal"><div class="modal-box" id="modalBox"></div></div>
<script>
(function(){
"use strict";
function $(s,r){return (r||document).querySelector(s);}
function esc(v){if(v===null||v===undefined)return "";return String(v).split("&").join("&amp;").split("<").join("&lt;").split(">").join("&gt;").split('"').join("&quot;").split("'").join("&#39;");}
function num(v,d){var n=Number(v);return isFinite(n)?n:(d===undefined?0:d);}
function enc(v){return encodeURIComponent(String(v===undefined||v===null?"":v));}
function valOf(f,n){var el=f.elements[n];return el?String(el.value||"").trim():"";}
)LF";
inline const char* const kHtml3 = R"LF(function mkErr(msg,st){var e=new Error(msg);e.status=st||0;return e;}
function toast(msg,kind){var t=$("#toast");if(!t)return;t.textContent=msg;t.className="toast show"+(kind?" "+kind:"");clearTimeout(toast._t);toast._t=setTimeout(function(){t.className="toast";},3200);}
function fail(e){toast((e&&e.message)||"操作失败","err");}

/* ---------------- 全局状态 ---------------- */
var S={user:null,unread:0,metaLoaded:false,meta:{categories:[],locations:[],types:[]},es:null,homeDirty:false};
var HOME={type:"",categoryId:"",locationId:"",status:"active",q:"",page:1,pageSize:12};
var DET={pid:"",item:null,editing:false,claims:[]};
var ST_OPTS=[["active","全部有效"],["open","进行中"],["claim_pending","认领待审核"],["claim_approved","已通过待交接"],["completed","已完成"],["expired","已过期"],["removed","已下架"],["all","全部状态"]];
var LOGINTAB="login",MYTAB="items",R_PATH="",R_ARG="";

function loadingHtml(txt){return '<div class="loading"><span class="spin"></span>'+esc(txt||"加载中…")+'</div>';}
function emptyHtml(icon,title,text,btn){return '<div class="empty"><div class="eicon">'+icon+'</div><h3>'+esc(title)+'</h3><p>'+esc(text||"")+'</p>'+(btn||"")+'</div>';}
function render(html){var v=$("#view");if(v)v.innerHTML=html;}
function statusBadge(st,text){return '<span class="badge st'+num(st,0)+'">'+esc(text||"")+'</span>';}
function typeBadge(tp,text){return '<span class="badge ty'+num(tp,0)+'">'+esc(text||"")+'</span>';}
function openModal(html){var m=$("#modal"),b=$("#modalBox");if(!m||!b)return;b.innerHTML=html;m.className="modal";}
function closeModal(){var m=$("#modal");if(m)m.className="modal hide";}
function localDT(d){var p=function(n){return (n<10?"0":"")+n;};return d.getFullYear()+"-"+p(d.getMonth()+1)+"-"+p(d.getDate())+"T"+p(d.getHours())+":"+p(d.getMinutes());}

/* ---------------- 网络请求 ---------------- */
async function api(method,path,body,opts){
  opts=opts||{};
  var init={method:method,credentials:"same-origin",headers:{}};
  if(method!=="GET")init.headers["X-Requested-With"]="CampusLF";
  if(body instanceof FormData){init.body=body;}
  else if(body!==undefined&&body!==null){init.headers["Content-Type"]="application/json";init.body=JSON.stringify(body);}
  var res;
  try{res=await fetch(path,init);}catch(e){throw mkErr("网络连接失败，请稍后重试",0);}
  var data=null;
  try{data=await res.json();}catch(e){data=null;}
  if(res.status===401){
    if(!opts.silent){logoutLocal();if(location.hash.indexOf("#/login")!==0)location.hash="#/login";}
    throw mkErr((data&&data.error&&data.error.message)||"请先登录",401);
  }
  if(!data||data.ok!==true)throw mkErr((data&&data.error&&data.error.message)||("请求失败（HTTP "+res.status+"）"),res.status);
  return data.data;
}
async function safeGet(path,def){try{return await api("GET",path);}catch(e){return def;}}

/* ---------------- 顶栏 / 登录状态 / SSE ---------------- */
function logoutLocal(){S.user=null;S.unread=0;if(S.es){try{S.es.close();}catch(e){}S.es=null;}updateTopbar();}
function setUser(u){if(u){S.user=u;connectStream();}else{logoutLocal();}updateTopbar();}
function updateTopbar(){
  var nav=$("#nav"),tbr=$("#tbr"),tab=$("#tabbar");
  if(!nav||!tbr)return;
  var links;
  if(S.user){links=[["#/items","首页"],["#/publish","发布"],["#/my","我的"],["#/notifications","通知"],["#/me","资料"]];if(S.user.role==="admin")links.splice(4,0,["#/admin","管理"]);}
  else{links=[["#/items","首页"],["#/login","登录 / 注册"]];}
  var cur=location.hash||"#/items";
  nav.innerHTML=links.map(function(l){return '<a href="'+l[0]+'" class="'+(cur.indexOf(l[0])===0?"on":"")+'">'+esc(l[1])+'</a>';}).join("");
  if(S.user){
    var n=num(S.unread,0);
    tbr.innerHTML='<a class="bell" href="#/notifications" title="通知">🔔<b class="'+(n>0?"":"zero")+'">'+(n>99?"99+":n)+'</b></a><a class="who" href="#/me">'+esc(S.user.name)+'</a>';
  }else{
    tbr.innerHTML='<a class="btn sm" href="#/login">登录 / 注册</a>';
  }
  if(tab){
    tab.innerHTML=S.user?'<a href="#/items">🏠<span>首页</span></a><a href="#/publish">➕<span>发布</span></a><a href="#/my">📋<span>我的</span></a><a href="#/notifications">🔔<span>通知</span></a><a href="#/me">👤<span>资料</span></a>':'<a href="#/login">🔑<span>登录</span></a>';
  }
}
function connectStream(){
  if(!S.user)return;
  if(S.es){try{S.es.close();}catch(e){}S.es=null;}
  var es;
  try{es=new EventSource("/api/stream");}catch(e){return;}
  S.es=es;
  es.onmessage=function(ev){onEvent(ev.data);};
  es.onerror=function(){
    if(!S.user)return;
    api("GET","/api/auth/me",undefined,{silent:true}).then(function(d){
      if(d&&d.user){S.unread=num(d.unreadCount,0);updateTopbar();}
    }).catch(function(e){
      if(e&&e.status===401){logoutLocal();if(location.hash.indexOf("#/login")!==0)location.hash="#/login";}
    });
  };
}
function onEvent(raw){
  var d;
  try{d=JSON.parse(raw);}catch(e){return;}
  if(!d||!d.type)return;
  var t=d.type;
  if(t==="notification"){
    var n=d.notification||{};
    S.unread=num(d.unreadCount,num(S.unread,0)+1);
    updateTopbar();
    toast("🔔 "+(n.title||"收到新通知"),"info");
    if(R_PATH==="/notifications")route();
  }else if(t==="item-created"){
    S.homeDirty=true;
    toast("📌 新发布："+(d.title||"新物品"),"info");
    if(R_PATH==="/items")loadList();
  }else if(t==="item-updated"){
    S.homeDirty=true;
    if(R_PATH==="/item"&&R_ARG===d.publicId)renderDetail(R_ARG);
    if(R_PATH==="/items")loadList();
    if(R_PATH==="/my")route();
  }else if(t==="claim-created"){
    toast("📥 「"+(d.itemTitle||"物品")+"」收到新的认领申请","info");
    if(R_PATH==="/item"&&R_ARG===d.itemPublicId)renderDetail(R_ARG);
  }else if(t==="claim-updated"){
    toast("认领状态已更新","info");
    if(R_PATH==="/item"&&R_ARG===d.itemPublicId)renderDetail(R_ARG);
    if(R_PATH==="/my"||R_PATH==="/notifications")route();
  }
}

/* ---------------- 路由 ---------------- */
async function route(){
  var raw=location.hash.slice(1)||"/items";
  var clean=raw.split("?")[0];
  var seg=clean.split("/").filter(function(x){return x;});
  R_PATH="/"+(seg[0]||"items");
  R_ARG=seg[1]?decodeURIComponent(seg[1]):"";
  window.scrollTo(0,0);
  try{
    if(R_PATH==="/login"){
      if(S.user){location.hash="#/items";return;}
      renderLogin();
    }else if(R_PATH==="/items")await renderHome();
    else if(R_PATH==="/item")await renderDetail(R_ARG);
    else if(R_PATH==="/publish")await renderPublish();
    else if(R_PATH==="/my")await renderMy();
    else if(R_PATH==="/notifications")await renderNotifications();
    else if(R_PATH==="/admin")await renderAdmin();
    else if(R_PATH==="/me")await renderMe();
    else render(emptyHtml("🧭","页面不存在","请检查地址是否正确",'<a class="btn" href="#/items">回到首页</a>'));
  }catch(e){render(emptyHtml("⚠️","页面加载失败",(e&&e.message)||"未知错误",'<a class="btn" href="#/items">回到首页</a>'));}
  updateTopbar();
}
function needLogin(){if(!S.user){toast("请先登录","warn");location.hash="#/login";return false;}return true;}

/* ---------------- 首页列表 ---------------- */
async function ensureMeta(){
  if(S.metaLoaded)return;
  try{
    var d=await api("GET","/api/meta",undefined,{silent:true});
    S.meta.categories=d.categories||[];
    S.meta.locations=d.locations||[];
    S.meta.types=d.types||[];
    S.metaLoaded=true;
  }catch(e){}
}
function fselect(kind,placeholder,list,cur){
  var h='<select class="input fl" data-filter="'+kind+'"><option value="">'+esc(placeholder)+'</option>';
  (list||[]).forEach(function(o){
    var v=(o.id!==undefined&&o.id!==null)?o.id:o.name;
    var label=o.building?(o.name+" · "+o.building):o.name;
    h+='<option value="'+esc(v)+'"'+(String(v)===String(cur)?" selected":"")+'>'+esc(label)+'</option>';
  });
  return h+'</select>';
}
function statusSelect(){
  var h='<select class="input fl" data-filter="status">';
  ST_OPTS.forEach(function(o){h+='<option value="'+o[0]+'"'+(o[0]===HOME.status?" selected":"")+'>'+esc(o[1])+'</option>';});
  return h+'</select>';
}
function cardHtml(it){
  var imgs=it.images||[];
)LF";
inline const char* const kHtml4 = R"LF(  var thumb=imgs.length?'<img src="'+esc(imgs[0].url)+'" alt="物品图片" loading="lazy">':'<span class="ph">'+(num(it.type,1)===0?"🔍":"🎁")+'</span>';
  return '<a class="card" href="#/item/'+esc(it.publicId)+'">'+
    '<div class="thumb">'+thumb+typeBadge(it.type,it.typeText)+'</div>'+
    '<div class="cbody"><div class="crow">'+statusBadge(it.status,it.statusText)+'</div>'+
    '<h3 class="ctitle">'+esc(it.title)+'</h3>'+
    '<p class="cli">📍 '+esc(it.locationName||"—")+'</p>'+
    '<p class="cli">🗓 '+esc(it.eventTime||"—")+'</p>'+
    '<div class="cfoot"><span>'+esc(it.createdAtText||"")+'</span><span>认领 '+num(it.claimCount,0)+'</span></div>'+
    '</div></a>';
}
async function renderHome(){
  await ensureMeta();
  var types=[{id:"",name:"全部"}].concat(S.meta.types||[]);
  var h='<section class="hero"><div><h2>发现身边的失物与招领</h2><p class="sub">共 <b id="totalNum">0</b> 条信息 · 实时更新</p></div><a class="btn primary" href="#/publish">＋ 发布信息</a></section>';
  h+='<div class="panel">';
  h+='<div class="tabs">'+types.map(function(t){return '<button type="button" class="tab'+(String(t.id)===String(HOME.type)?" on":"")+'" data-act="home-type" data-type="'+esc(t.id)+'">'+esc(t.name)+'</button>';}).join("")+'</div>';
  h+='<div class="filters">';
  h+=fselect("categoryId","全部分类",S.meta.categories,HOME.categoryId);
  h+=fselect("locationId","全部地点",S.meta.locations,HOME.locationId);
  h+=statusSelect();
  h+='<form class="searchbox" data-form="search"><input class="input" name="q" placeholder="搜索标题或描述关键词" value="'+esc(HOME.q)+'"><button class="btn" type="submit">搜索</button></form>';
  h+='</div></div>';
  h+='<div class="listmeta"><span id="listInfo"></span><span id="dirtyMark" class="dotmark" style="display:none">有新信息</span></div>';
  h+='<div id="listBox">'+loadingHtml()+'</div>';
  h+='<div id="pager"></div>';
  render(h);
  await loadList();
}
function pagerHtml(total,page,pageSize){
  var pages=Math.max(1,Math.ceil(total/pageSize));
  var start=Math.max(1,Math.min(page-2,pages-4));
  var end=Math.min(pages,start+4);
  var h='<div class="pager"><button type="button" data-act="page" data-page="'+(page-1)+'"'+(page<=1?" disabled":"")+'>上一页</button><span class="pages">';
  for(var i=start;i<=end;i++)h+='<button type="button" class="'+(i===page?"on":"")+'" data-act="page" data-page="'+i+'">'+i+'</button>';
  h+='</span><button type="button" data-act="page" data-page="'+(page+1)+'"'+(page>=pages?" disabled":"")+'>下一页</button><span>共 '+total+' 条 · 第 '+page+'/'+pages+' 页</span></div>';
  return h;
}
async function loadList(){
  var box=$("#listBox");
  if(!box)return;
  box.innerHTML=loadingHtml();
  var qs="?type="+enc(HOME.type)+"&categoryId="+enc(HOME.categoryId)+"&locationId="+enc(HOME.locationId)+"&status="+enc(HOME.status)+"&q="+enc(HOME.q)+"&page="+HOME.page+"&pageSize="+HOME.pageSize;
  var d;
  try{d=await api("GET","/api/items"+qs);}catch(e){box.innerHTML=emptyHtml("⚠️","加载失败",e.message);return;}
  var items=d.items||[];
  var total=num(d.total,items.length);
  S.homeDirty=false;
  var info=$("#listInfo");
  if(info)info.textContent="共 "+total+" 条";
  var tn=$("#totalNum");
  if(tn)tn.textContent=String(total);
  var dm=$("#dirtyMark");
  if(dm)dm.style.display="none";
  if(!items.length)box.innerHTML=emptyHtml("🗂","暂无相关信息","换个关键词或筛选条件试试");
  else box.innerHTML='<div class="grid">'+items.map(cardHtml).join("")+'</div>';
  var pg=$("#pager");
  if(pg)pg.innerHTML=items.length?pagerHtml(total,HOME.page,HOME.pageSize):"";
}

/* ---------------- 详情页 ---------------- */
async function renderDetail(pid){
  if(!pid){render(emptyHtml("🧭","参数错误","缺少物品编号",'<a class="btn" href="#/items">回到首页</a>'));return;}
  render(loadingHtml());
  var d;
  try{d=await api("GET","/api/items/"+enc(pid));}catch(e){render(emptyHtml("⚠️","无法打开该物品",e.message,'<a class="btn" href="#/items">回列表</a>'));return;}
  DET.pid=pid;
  DET.item=d.item;
  DET.editing=false;
  DET.claims=[];
  renderDetailView();
}
function catOpts(sel){
  var list=S.meta.categories||[];
  var h='<select class="input" name="categoryId">';
  if(!list.length)h+='<option value="0">请选择分类</option>';
  list.forEach(function(c){h+='<option value="'+c.id+'"'+(String(c.id)===String(sel)?" selected":"")+'>'+esc(c.name)+'</option>';});
  return h+'</select>';
}
function locOpts(sel){
  var list=S.meta.locations||[];
  var h='<select class="input" name="locationId">';
  if(!list.length)h+='<option value="0">请选择地点</option>';
  list.forEach(function(l){h+='<option value="'+l.id+'"'+(String(l.id)===String(sel)?" selected":"")+'>'+esc(l.name+(l.building?" · "+l.building:""))+'</option>';});
  return h+'</select>';
}
function claimForm(it){
  var h='<form class="block claimform" data-form="claim"><h3>我要认领</h3>';
  if(it.verifyQuestion)h+='<label class="flabel">核验问题回答 <span class="req">*</span><input class="input" name="answer" maxlength="200" placeholder="'+esc(it.verifyQuestion)+'"></label>';
  h+='<label class="flabel">补充说明（可选）<input class="input" name="message" maxlength="300" placeholder="补充物品细节，方便发布者确认"></label>';
  h+='<button class="btn primary" type="submit">提交认领申请</button></form>';
  return h;
}
function detailActions(it){
  if(!S.user)return '<div class="notice">登录后即可提交认领申请 <a class="btn sm primary" href="#/login" style="margin-left:8px">去登录</a></div>';
  if(it.canManage){
    var h='<button type="button" class="btn" data-act="edit-open">✏️ 编辑</button>';
    if(DET.editing)h+='<button type="button" class="btn" data-act="edit-cancel">取消编辑</button>';
    h+='<button type="button" class="btn danger" data-act="item-delete" data-pid="'+esc(it.publicId)+'">🚫 下架</button>';
    return h;
  }
  var out="";
  if(it.claimAllowed)out+=claimForm(it);
  else if(num(it.status,0)===0)out+='<div class="notice">你已提交过认领申请，请等待发布者审核</div>';
  else out+='<div class="notice">该物品当前不可认领（'+esc(it.statusText||"")+'）</div>';
  out+='<button type="button" class="btn" data-act="report-open" data-pid="'+esc(it.publicId)+'">🚩 举报</button>';
  return out;
}
function editForm(it){
  var h='<form class="block editform" data-form="edit-item"><h3>编辑信息</h3>';
  h+='<label class="flabel">标题<input class="input" name="title" maxlength="60" value="'+esc(it.title)+'"></label>';
  h+='<label class="flabel">描述<textarea class="input" name="description" rows="4" maxlength="2000">'+esc(it.description)+'</textarea></label>';
  h+='<div class="frow"><label class="flabel">分类'+catOpts(it.categoryId)+'</label><label class="flabel">地点'+locOpts(it.locationId)+'</label></div>';
  h+='<label class="flabel">发生时间<input class="input" name="eventTime" maxlength="40" value="'+esc(it.eventTime||"")+'" placeholder="2026-11-08 14:30"></label>';
  h+='<label class="flabel">核验问题<input class="input" name="verifyQuestion" maxlength="120" value="'+esc(it.verifyQuestion||"")+'"></label>';
  h+='<div class="fbtns"><button class="btn primary" type="submit">保存修改</button><button class="btn" type="button" data-act="edit-cancel">取消</button></div></form>';
  return h;
}
function renderDetailView(){
  var it=DET.item;
  if(!it)return;
  var imgs=it.images||[];
  var h='<a class="back" href="#/items">← 返回列表</a><div class="detail">';
  h+='<div class="dgallery">'+(imgs.length?imgs.map(function(im,i){return '<button type="button" class="gimg" data-act="zoom" data-src="'+esc(im.url)+'"><img src="'+esc(im.url)+'" alt="图片'+(i+1)+'"></button>';}).join(""):'<div class="gph">🧭 暂无图片</div>')+'</div>';
  h+='<div class="dmain">';
)LF";
inline const char* const kHtml5 = R"LF(  h+='<div class="chips">'+typeBadge(it.type,it.typeText)+statusBadge(it.status,it.statusText)+'</div>';
  h+='<h1 class="dtitle">'+esc(it.title)+'</h1>';
  h+='<p class="dsub">'+esc(it.createdAtText||"")+' · 浏览 '+num(it.viewCount,0)+' · 认领 '+num(it.claimCount,0)+'</p>';
  h+='<dl class="info">';
  h+='<div><dt>分类</dt><dd>'+esc(it.categoryName||"—")+'</dd></div>';
  h+='<div><dt>地点</dt><dd>'+esc(it.locationName||"—")+(it.locationBuilding?" · "+esc(it.locationBuilding):"")+'</dd></div>';
  h+='<div><dt>发生时间</dt><dd>'+esc(it.eventTime||"—")+'</dd></div>';
  h+='<div><dt>发布者</dt><dd>'+esc((it.owner&&it.owner.name)||"—")+(it.owner&&it.owner.college?' · '+esc(it.owner.college):"")+'</dd></div>';
  h+='</dl>';
  h+='<section class="block"><h3>物品描述</h3><p class="desc">'+esc(it.description||"")+'</p></section>';
  if(it.verifyQuestion)h+='<section class="block qbox"><h3>核验问题</h3><p>'+esc(it.verifyQuestion)+'</p></section>';
  h+='<div class="dactions">'+detailActions(it)+'</div>';
  if(DET.editing)h+=editForm(it);
  if(it.canManage){
    h+='<section class="block"><div class="sechead"><h3>认领申请</h3><span class="muted">通过后生成 6 位取件码</span></div><div id="claimsBox">'+loadingHtml()+'</div>';
    h+='<form class="vform" data-form="verify"><input class="input code" name="code" inputmode="numeric" maxlength="6" placeholder="输入 6 位取件码核销"><button class="btn primary" type="submit">核销</button></form></section>';
  }
  h+='</div></div>';
  render(h);
  if(it.canManage)loadClaims(DET.pid);
}
function claimRow(c){
  var h='<div class="claimrow"><div class="crhead"><b>'+esc(c.claimantName||"匿名")+'</b>'+statusBadge(c.status,c.statusText)+'<span class="muted">'+esc(c.createdAtText||"")+'</span></div>';
  if(c.answer)h+='<p class="crline"><span>回答：</span>'+esc(c.answer)+'</p>';
  if(c.message)h+='<p class="crline"><span>说明：</span>'+esc(c.message)+'</p>';
  if(c.pickupCode)h+='<p class="crline"><span>取件码：</span><b class="code">'+esc(c.pickupCode)+'</b></p>';
  h+='<div class="crbtns">';
  if(c.canReview)h+='<button type="button" class="btn sm primary" data-act="claim-approve" data-id="'+esc(c.id)+'">通过</button><button type="button" class="btn sm danger" data-act="claim-reject" data-id="'+esc(c.id)+'">拒绝</button>';
  h+='</div></div>';
  return h;
}
async function loadClaims(pid){
  var box=$("#claimsBox");
  if(!box)return;
  var d;
  try{d=await api("GET","/api/items/"+enc(pid)+"/claims");}catch(e){box.innerHTML='<p class="muted">'+esc(e.message)+'</p>';return;}
  var cs=d.claims||[];
  DET.claims=cs;
  if(!cs.length){box.innerHTML='<p class="muted">暂无认领申请</p>';return;}
  box.innerHTML=cs.map(claimRow).join("");
}

/* ---------------- 发布页 ---------------- */
async function renderPublish(){
  if(!needLogin())return;
  await ensureMeta();
  if(!S.meta.categories||!S.meta.categories.length){
    render(loadingHtml("正在加载分类与地点…"));
    await ensureMeta();
  }
  var h='<a class="back" href="#/items">← 返回列表</a><div class="authwrap"><div class="authcard wide"><h2>发布信息</h2><p class="muted">填写物品信息，帮助失物回家</p>';
  h+='<form class="pform" data-form="publish">';
  h+='<label class="flabel">类型<select class="input" name="type"><option value="0">寻物（我丢了东西）</option><option value="1">招领（我捡到东西）</option></select></label>';
  h+='<label class="flabel">标题<input class="input" name="title" maxlength="60" placeholder="例如：图书馆二楼捡到一张校园卡"></label>';
  h+='<label class="flabel">描述<textarea class="input" name="description" rows="5" maxlength="2000" placeholder="请描述物品特征、拾取或丢失的经过"></textarea></label>';
  h+='<div class="frow"><label class="flabel">分类'+catOpts(0)+'</label><label class="flabel">地点'+locOpts(0)+'</label></div>';
  h+='<div class="frow"><label class="flabel">发生时间<input class="input" type="datetime-local" name="eventTime" value="'+localDT(new Date())+'"></label><label class="flabel">有效天数<input class="input" type="number" name="expiresInDays" min="1" max="90" value="30"></label></div>';
  h+='<label class="flabel">核验问题（可选）<input class="input" name="verifyQuestion" maxlength="120" placeholder="认领时需要回答的问题，例如：卡背面写了什么"></label>';
  h+='<label class="flabel">图片（最多 3 张，jpg / png / webp，单张不超过 5MB）<input class="input" type="file" name="images" id="imgInput" accept="image/jpeg,image/png,image/webp" multiple></label>';
  h+='<div class="previews" id="previews"><span class="muted">未选择图片</span></div>';
  h+='<p class="hint">请勿上传含人脸、证件号码、家庭住址或定位信息的照片；建议只拍物品本身并遮挡敏感信息。</p>';
  h+='<button class="btn primary block" type="submit">发布</button></form></div></div>';
  render(h);
  bindPublish();
}
function bindPublish(){
  var inp=$("#imgInput");
  if(!inp)return;
  inp.addEventListener("change",function(){
    var box=$("#previews");
    if(!box)return;
    var files=Array.prototype.slice.call(inp.files||[]);
    if(files.length>3){toast("最多选择 3 张图片，已取前 3 张","warn");files=files.slice(0,3);}
    if(!files.length){box.innerHTML='<span class="muted">未选择图片</span>';return;}
    box.innerHTML=files.map(function(f){return '<div class="pv"><img src="'+URL.createObjectURL(f)+'" alt="预览"><span>'+esc(f.name)+'</span></div>';}).join("");
  });
}

/* ---------------- 我的 ---------------- */
async function renderMy(){
  if(!needLogin())return;
  var h='<div class="pagehead"><h2>我的</h2><div class="tabs"><button type="button" class="tab'+(MYTAB==="items"?" on":"")+'" data-act="my-tab" data-tab="items">我发布的</button><button type="button" class="tab'+(MYTAB==="claims"?" on":"")+'" data-act="my-tab" data-tab="claims">我认领的</button></div></div>';
  h+='<div id="myBox">'+loadingHtml()+'</div>';
  render(h);
  await loadMy();
}
async function loadMy(){
  var box=$("#myBox");
  if(!box)return;
  box.innerHTML=loadingHtml();
  if(MYTAB==="items"){
    var d;
    try{d=await api("GET","/api/me/items?page=1&pageSize=50");}catch(e){box.innerHTML=emptyHtml("⚠️","加载失败",e.message);return;}
    var items=d.items||[];
    if(!items.length){box.innerHTML=emptyHtml("📝","还没有发布过信息","发布寻物或招领信息后会显示在这里",'<a class="btn" href="#/publish">去发布</a>');return;}
    box.innerHTML='<div class="rows">'+items.map(function(it){
      return '<div class="rowitem"><div class="rimain"><a class="rititle" href="#/item/'+esc(it.publicId)+'">'+esc(it.title)+'</a><div class="chips">'+typeBadge(it.type,it.typeText)+statusBadge(it.status,it.statusText)+'</div><p class="muted">'+esc(it.createdAtText||"")+' · 认领 '+num(it.claimCount,0)+'</p></div><div class="ribtns"><a class="btn sm" href="#/item/'+esc(it.publicId)+'">查看认领 / 管理</a><button type="button" class="btn sm danger" data-act="item-delete" data-pid="'+esc(it.publicId)+'">删除</button></div></div>';
    }).join("")+'</div>';
    return;
  }
  var c;
  try{c=await api("GET","/api/me/claims");}catch(e){box.innerHTML=emptyHtml("⚠️","加载失败",e.message);return;}
  var cs=c.claims||[];
  if(!cs.length){box.innerHTML=emptyHtml("📭","还没有认领记录","在物品详情页可以提交认领申请");return;}
  box.innerHTML='<div class="rows">'+cs.map(function(x){
    var h='<div class="rowitem"><div class="rimain"><a class="rititle" href="#/item/'+esc(x.itemPublicId)+'">'+esc(x.itemTitle||"物品")+'</a><div class="chips">'+typeBadge(x.itemType,"寻物/招领")+statusBadge(x.status,x.statusText)+'</div><p class="muted">申请于 '+esc(x.createdAtText||"")+'</p>';
    if(x.pickupCode)h+='<p class="pickup">取件码 <b>'+esc(x.pickupCode)+'</b>'+(x.pickupExpiresAt?'<span class="muted"> 有效期至 '+esc(x.pickupExpiresAt)+'</span>':"")+'</p>';
)LF";
inline const char* const kHtml6 = R"LF(    h+='</div><div class="ribtns">'+(x.canCancel?'<button type="button" class="btn sm danger" data-act="claim-cancel" data-id="'+esc(x.id)+'">取消申请</button>':"")+'<a class="btn sm" href="#/item/'+esc(x.itemPublicId)+'">查看详情</a></div></div>';
    return h;
  }).join("")+'</div>';
}

/* ---------------- 通知 ---------------- */
async function renderNotifications(){
  if(!needLogin())return;
  render(loadingHtml());
  var d;
  try{d=await api("GET","/api/notifications?unreadOnly=0&limit=50");}catch(e){render(emptyHtml("⚠️","加载失败",e.message));return;}
  S.unread=num(d.unreadCount,0);
  updateTopbar();
  renderNotifList(d.notifications||[]);
}
function renderNotifList(list){
  var h='<div class="pagehead"><h2>通知中心</h2><button type="button" class="btn sm" data-act="notif-all">全部标为已读</button></div>';
  if(!list.length){h+=emptyHtml("🔔","暂无通知","有新的认领或状态变化时会在这里提醒你");render(h);return;}
  h+='<div class="notifs">'+list.map(function(n){
    return '<button type="button" class="notif'+(n.read?"":" unread")+'" data-act="notif-open" data-id="'+esc(n.id)+'" data-link="'+esc(n.link||"")+'"><span class="nicon">'+(n.read?"📩":"🆕")+'</span><span class="nbody"><span class="ntitle">'+esc(n.title||"通知")+'</span><span class="ntext">'+esc(n.body||"")+'</span><span class="ntime">'+esc(n.createdAtText||"")+'</span></span></button>';
  }).join("")+'</div>';
  render(h);
}

/* ---------------- 管理后台 ---------------- */
async function renderAdmin(){
  if(!needLogin())return;
  if(!S.user||S.user.role!=="admin"){
    render(emptyHtml("🔒","无权限访问","该页面仅管理员可见",'<a class="btn" href="#/items">返回首页</a>'));
    return;
  }
  render(loadingHtml("正在加载管理数据…"));
  var st=await safeGet("/api/admin/stats",{stats:{}});
  var rp=await safeGet("/api/admin/reports",{reports:[]});
  var iv=await safeGet("/api/admin/invites",{invites:[]});
  var au=await safeGet("/api/admin/audit?limit=100",{logs:[]});
  var s=st.stats||{};
  var h='<div class="pagehead"><h2>管理后台</h2><span class="muted">管理员：'+esc(S.user.name)+'</span></div>';
  h+='<div class="stats">'+[["用户",s.users],["物品",s.items],["进行中",s.openItems],["待审认领",s.pendingClaims],["完成认领",s.completedClaims],["待处理举报",s.openReports]].map(function(x){return '<div class="stat"><b>'+num(x[1],0)+'</b><span>'+esc(x[0])+'</span></div>';}).join("")+'</div>';
  var reps=rp.reports||[];
  h+='<section class="panel"><div class="sechead"><h3>举报处理</h3><span class="muted">'+reps.length+' 条</span></div>';
  if(!reps.length)h+='<p class="muted">暂无举报</p>';
  else h+='<div class="rows">'+reps.map(function(r){
    var b="";
    if(num(r.status,0)===0)b='<button type="button" class="btn sm primary" data-act="admin-report" data-id="'+esc(r.id)+'" data-action="resolve">处理</button><button type="button" class="btn sm" data-act="admin-report" data-id="'+esc(r.id)+'" data-action="dismiss">忽略</button>';
    return '<div class="rowitem"><div class="rimain"><b>'+esc(r.itemTitle||"物品")+'</b><p class="muted">'+esc(r.reason||"")+' · 举报人 '+esc(r.reporterName||"—")+' · '+esc(r.createdAtText||"")+'</p></div><div class="ribtns">'+statusBadge(r.status,r.statusText)+b+'</div></div>';
  }).join("")+'</div>';
  h+='</section>';
  h+='<section class="panel"><div class="sechead"><h3>邀请码</h3></div>';
  h+='<form class="vform" data-form="invite"><input class="input" name="maxUses" type="number" min="1" max="1000" value="50" placeholder="可用次数"><input class="input" name="validDays" type="number" min="1" max="365" value="30" placeholder="有效天数"><button class="btn primary" type="submit">生成邀请码</button></form>';
  var invs=iv.invites||[];
  if(!invs.length)h+='<p class="muted" style="margin-top:10px">暂无邀请码</p>';
  else h+='<div class="invs" style="margin-top:10px">'+invs.map(function(v){return '<div class="inv"><code>'+esc(v.codeMasked||"已隐藏")+'</code><span class="muted">'+num(v.uses,0)+' / '+num(v.maxUses,0)+' 次 · 到期 '+esc(v.expiresAtText||"—")+'</span>'+(v.disabled?'<span class="badge st5">已禁用</span>':"")+'</div>';}).join("")+'</div><p class="hint">完整邀请码仅在创建时显示一次，列表只显示掩码；请创建后立即复制保存。</p>';
  h+='</section>';
  h+='<section class="panel"><div class="sechead"><h3>物品搜索 / 下架</h3></div>';
  h+='<form class="vform" data-form="admin-search"><input class="input" name="q" placeholder="输入标题关键词（留空列出最近物品）" value="'+esc(ADMINQ)+'"><button class="btn" type="submit">搜索</button></form><div id="adminItems"></div></section>';
  var logs=au.logs||[];
  h+='<section class="panel"><div class="sechead"><h3>审计日志</h3><span class="muted">最近 '+logs.length+' 条</span></div>';
  h+=logs.length?'<div class="logs">'+logs.map(function(l){return '<div class="log"><span class="lact">'+esc(l.action||"")+'</span><span class="ldet">'+esc(l.detail||"")+'</span><span class="lm">'+esc(l.actorName||"")+' · '+esc(l.ip||"")+' · '+esc(l.createdAtText||"")+'</span></div>';}).join("")+'</div>':'<p class="muted">暂无日志</p>';
  h+='</section>';
  render(h);
  if(ADMINQ)await adminSearch(ADMINQ);
}
var ADMINQ="";
async function adminSearch(q){
  var box=$("#adminItems");
  if(!box)return;
  ADMINQ=q;
  box.innerHTML=loadingHtml();
  var d;
  try{d=await api("GET","/api/items?status=all&pageSize=20&q="+enc(q));}catch(e){box.innerHTML='<p class="muted">'+esc(e.message)+'</p>';return;}
  var items=d.items||[];
  if(!items.length){box.innerHTML='<p class="muted">没有匹配的物品</p>';return;}
  box.innerHTML='<div class="rows" style="margin-top:10px">'+items.map(function(it){
    return '<div class="rowitem"><div class="rimain"><a class="rititle" href="#/item/'+esc(it.publicId)+'">'+esc(it.title)+'</a><div class="chips">'+typeBadge(it.type,it.typeText)+statusBadge(it.status,it.statusText)+'</div><p class="muted">'+esc(it.createdAtText||"")+' · '+esc(it.locationName||"")+'</p></div><div class="ribtns"><button type="button" class="btn sm danger" data-act="admin-takedown" data-pid="'+esc(it.publicId)+'">下架</button></div></div>';
  }).join("")+'</div>';
}

/* ---------------- 我的资料 ---------------- */
async function renderMe(){
  if(!needLogin())return;
  render(loadingHtml());
  var d;
  try{d=await api("GET","/api/auth/me");}catch(e){d=null;}
  if(d){S.user=d.user;S.unread=num(d.unreadCount,0);updateTopbar();}
  var u=S.user;
  if(!u){render(emptyHtml("⚠️","无法获取资料","请重新登录"));return;}
  var h='<div class="pagehead"><h2>我的资料</h2></div><div class="profile"><div class="avatar">'+esc((u.name||"?").slice(0,1))+'</div>';
  h+='<div class="pinfo"><h3>'+esc(u.name||"")+' <span class="badge '+(u.role==="admin"?"ty1":"ty0")+'">'+(u.role==="admin"?"管理员":"学生")+'</span></h3>';
  h+='<dl class="info"><div><dt>学号</dt><dd>'+esc(u.studentNo||"")+'</dd></div><div><dt>姓名</dt><dd>'+esc(u.name||"")+'</dd></div><div><dt>学院</dt><dd>'+esc(u.college||"")+'</dd></div><div><dt>注册时间</dt><dd>'+esc(u.createdAt||"")+'</dd></div></dl></div></div>';
  h+='<section class="panel"><h3>修改密码</h3><form data-form="password"><label class="flabel">当前密码<input class="input" type="password" name="oldPassword" autocomplete="current-password"></label><label class="flabel">新密码<input class="input" type="password" name="newPassword" autocomplete="new-password" placeholder="至少 6 位"></label><label class="flabel">确认新密码<input class="input" type="password" name="confirm" autocomplete="new-password"></label><div class="fbtns"><button class="btn primary" type="submit">保存修改</button></div></form></section>';
  h+='<section class="panel"><h3>账号操作</h3><p class="muted">退出后需要重新登录才能认领或发布信息。</p><div class="fbtns"><button type="button" class="btn danger" data-act="logout">退出登录</button></div></section>';
  render(h);
}

)LF";
inline const char* const kHtml7 = R"LF(/* ---------------- 登录 / 注册 ---------------- */
function renderLogin(){
  var isLogin=LOGINTAB==="login";
  var form;
  if(isLogin){
    form='<form data-form="login"><label class="flabel">学号<input class="input" name="studentNo" autocomplete="username" placeholder="请输入学号" required></label><label class="flabel">密码<input class="input" type="password" name="password" autocomplete="current-password" placeholder="请输入密码" required></label><div class="fbtns"><button class="btn primary block" type="submit">登录</button></div></form>';
  }else{
    form='<form data-form="register"><label class="flabel">邀请码<input class="input" name="inviteCode" autocomplete="off" spellcheck="false" placeholder="请输入管理员发放的邀请码" required></label><label class="flabel">学号<input class="input" name="studentNo" placeholder="4~20 位字母或数字" required></label><label class="flabel">姓名<input class="input" name="name" maxlength="20" placeholder="真实姓名" required></label><label class="flabel">学院<input class="input" name="college" maxlength="30" placeholder="例如：计算机学院" required></label><label class="flabel">密码<input class="input" type="password" name="password" autocomplete="new-password" placeholder="至少 6 位" required></label><div class="fbtns"><button class="btn primary block" type="submit">注册并登录</button></div></form>';
  }
  render('<div class="authwrap"><div class="authcard"><div class="authhead"><div class="alogo">🧭</div><h1>校园失物招领</h1><p class="muted">寻找失物 · 传递善意</p></div><div class="tabs center"><button type="button" class="tab'+(isLogin?" on":"")+'" data-act="login-tab" data-tab="login">登录</button><button type="button" class="tab'+(isLogin?"":" on")+'" data-act="login-tab" data-tab="register">注册</button></div>'+form+'<p class="hint">'+(isLogin?"管理员初始密码见服务端控制台（仅首次启动显示一次）":"邀请码由管理员发放，请向管理员获取；请勿公开传播")+'</p></div></div>');
}

/* ---------------- 业务操作 ---------------- */
async function doLogin(f){
  var studentNo=valOf(f,"studentNo"),password=valOf(f,"password");
  if(!studentNo||!password)throw mkErr("请输入学号和密码",400);
  await api("POST","/api/auth/login",{studentNo:studentNo,password:password});
  var me=await api("GET","/api/auth/me",undefined,{silent:true});
  setUser(me.user);
  S.unread=num(me.unreadCount,0);
  updateTopbar();
  toast("登录成功，欢迎回来 "+me.user.name,"ok");
  if(location.hash==="#/login"||!location.hash)location.hash="#/items";
  else route();
}
async function doRegister(f){
  var inviteCode=valOf(f,"inviteCode"),studentNo=valOf(f,"studentNo"),name=valOf(f,"name"),college=valOf(f,"college"),password=valOf(f,"password");
  if(!inviteCode||!studentNo||!name||!college||!password)throw mkErr("请填写完整的注册信息",400);
  if(password.length<6)throw mkErr("密码至少 6 位",400);
  await api("POST","/api/auth/register",{inviteCode:inviteCode,studentNo:studentNo,name:name,college:college,password:password});
  var me=await api("GET","/api/auth/me",undefined,{silent:true});
  setUser(me.user);
  S.unread=num(me.unreadCount,0);
  updateTopbar();
  toast("注册成功，欢迎 "+me.user.name,"ok");
  location.hash="#/items";
}
async function doClaim(f){
  if(!S.user)throw mkErr("请先登录",401);
  var it=DET.item;
  if(!it)throw mkErr("物品信息已失效，请刷新页面",400);
  var answer=valOf(f,"answer"),message=valOf(f,"message");
  if(it.verifyQuestion&&!answer)throw mkErr("请回答核验问题",400);
  await api("POST","/api/items/"+enc(DET.pid)+"/claims",{answer:answer,message:message});
  toast("认领申请已提交，等待发布者审核","ok");
  await renderDetail(DET.pid);
}
async function doEditItem(f){
  var it=DET.item;
  if(!it)throw mkErr("物品信息已失效，请刷新页面",400);
  var body={title:valOf(f,"title"),description:valOf(f,"description"),categoryId:num(valOf(f,"categoryId"),0),locationId:num(valOf(f,"locationId"),0),eventTime:valOf(f,"eventTime").replace("T"," "),verifyQuestion:valOf(f,"verifyQuestion")};
  if(body.title.length<2||body.title.length>60)throw mkErr("标题需 2~60 字",400);
  if(!body.description)throw mkErr("请填写物品描述",400);
  await api("PATCH","/api/items/"+enc(DET.pid),body);
  DET.editing=false;
  toast("已保存修改","ok");
  await renderDetail(DET.pid);
}
async function doPublish(f){
  var body={
    type:num(valOf(f,"type"),0),
    title:valOf(f,"title"),
    description:valOf(f,"description"),
    categoryId:num(valOf(f,"categoryId"),0),
    locationId:num(valOf(f,"locationId"),0),
    eventTime:valOf(f,"eventTime").replace("T"," "),
    verifyQuestion:valOf(f,"verifyQuestion"),
    expiresInDays:Math.min(90,Math.max(1,num(valOf(f,"expiresInDays"),30)))
  };
  if(body.title.length<2||body.title.length>60)throw mkErr("标题需 2~60 字",400);
  if(!body.description)throw mkErr("请填写物品描述",400);
  if(!body.categoryId)throw mkErr("请选择分类",400);
  if(!body.locationId)throw mkErr("请选择地点",400);
  var d=await api("POST","/api/items",body);
  var pid=d.item.publicId;
  var inp=f.elements.images;
  var files=inp&&inp.files?Array.prototype.slice.call(inp.files).slice(0,3):[];
  if(files.length){
    var fd=new FormData();
    files.forEach(function(fl){fd.append("images",fl);});
    try{await api("POST","/api/items/"+enc(pid)+"/images",fd);}
    catch(e){toast("图片上传失败："+e.message,"warn");}
  }
  toast("发布成功","ok");
  location.hash="#/item/"+pid;
}
async function doVerify(f){
  var code=valOf(f,"code");
  if(!(code.length===6&&isFinite(Number(code))))throw mkErr("请输入 6 位数字取件码",400);
  var target=null;
  (DET.claims||[]).forEach(function(c){if(num(c.status,0)===1)target=c;});
  if(!target)throw mkErr("当前没有待交接的认领申请",400);
  await api("POST","/api/claims/"+enc(target.id)+"/verify",{code:code});
  toast("核销成功，认领已完成","ok");
  await renderDetail(DET.pid);
}
async function doPassword(f){
  var oldp=valOf(f,"oldPassword"),np=valOf(f,"newPassword"),cp=valOf(f,"confirm");
  if(!oldp)throw mkErr("请输入当前密码",400);
  if(np.length<6)throw mkErr("新密码至少 6 位",400);
  if(np!==cp)throw mkErr("两次输入的新密码不一致",400);
  await api("POST","/api/auth/change-password",{oldPassword:oldp,newPassword:np});
  f.reset();
  toast("密码已修改","ok");
}
async function doInvite(f){
  var maxUses=Math.min(1000,Math.max(1,num(valOf(f,"maxUses"),50)));
  var validDays=Math.min(365,Math.max(1,num(valOf(f,"validDays"),30)));
  var d=await api("POST","/api/admin/invites",{maxUses:maxUses,validDays:validDays});
  var full=(d.invite&&d.invite.code)||"";
  openModal('<h3>邀请码已生成（仅显示一次）</h3><p class="muted">请立即复制保存；关闭后无法再次查看完整码，列表只显示掩码。</p><div style="font-size:22px;letter-spacing:1px;margin:14px 0;padding:12px;background:#f1faf7;border-radius:8px;text-align:center;word-break:break-all"><code>'+esc(full)+'</code></div><div class="fbtns"><button type="button" class="btn primary" data-act="modal-close">我已保存</button></div>');
  await renderAdmin();
}
async function doReport(f){
  var reason=valOf(f,"reason");
  if(reason.length<2)throw mkErr("请填写举报原因",400);
  var pid=(DET.item&&DET.item.publicId)||DET.pid;
  await api("POST","/api/reports",{targetPublicId:pid,reason:reason});
  closeModal();
  toast("举报已提交，管理员会尽快处理","ok");
}
async function doDelete(pid){
  if(!pid)return;
  if(!window.confirm("确定要下架/删除该物品吗？"))return;
  await api("DELETE","/api/items/"+enc(pid));
  toast("已下架该物品","ok");
  if(R_PATH==="/item")location.hash="#/items";
  else if(R_PATH==="/my")await loadMy();
  else if(R_PATH==="/admin")await renderAdmin();
  else await loadList();
}
async function doApprove(id){
  if(!id)return;
  var d=await api("POST","/api/claims/"+enc(id)+"/approve",{});
  var code=(d.claim&&d.claim.pickupCode)||d.pickupCode||"";
  toast(code?("已通过，取件码："+code):"已通过该认领申请","ok");
  await renderDetail(DET.pid);
}
async function doReject(id){
  if(!id)return;
  var reason=window.prompt("请输入拒绝原因（可留空）","")||"";
)LF";
inline const char* const kHtml8 = R"LF(  await api("POST","/api/claims/"+enc(id)+"/reject",{reason:reason});
  toast("已拒绝该申请","ok");
  await renderDetail(DET.pid);
}
async function doCancel(id){
  if(!id)return;
  if(!window.confirm("确定取消该认领申请吗？"))return;
  await api("POST","/api/claims/"+enc(id)+"/cancel",{});
  toast("已取消认领申请","ok");
  if(R_PATH==="/my")await loadMy();
  else if(DET.pid)await renderDetail(DET.pid);
  else route();
}
async function doNotifOpen(id,link,btn){
  try{
    if(btn&&btn.className.indexOf("unread")>=0){
      var d=await api("POST","/api/notifications/read",{ids:[num(id,0)],all:false});
      S.unread=num(d.unreadCount,Math.max(0,num(S.unread,0)-1));
      updateTopbar();
      btn.className="notif";
      btn.querySelector(".nicon").textContent="📩";
    }
  }catch(e){}
  if(link)location.hash=link;
}
async function doNotifAll(){
  var d=await api("POST","/api/notifications/read",{ids:[],all:true});
  S.unread=num(d.unreadCount,0);
  updateTopbar();
  toast("已全部标为已读","ok");
  await route();
}
async function doHandleReport(id,action){
  if(!id||!action)return;
  await api("POST","/api/admin/reports/"+enc(id)+"/handle",{action:action});
  toast(action==="resolve"?"已处理该举报":"已忽略该举报","ok");
  await renderAdmin();
}
async function doTakeDown(pid){
  if(!pid)return;
  var reason=window.prompt("请输入下架原因","信息不实")||"信息不实";
  await api("POST","/api/admin/items/"+enc(pid)+"/take-down",{reason:reason});
  toast("已下架该物品","ok");
  await renderAdmin();
}
async function doLogout(){
  try{await api("POST","/api/auth/logout",{},{silent:true});}catch(e){}
  logoutLocal();
  location.hash="#/login";
  toast("已退出登录","ok");
}

/* ---------------- 事件绑定 ---------------- */
document.addEventListener("submit",async function(e){
  var f=e.target;
  while(f&&f.getAttribute&&!f.getAttribute("data-form"))f=f.parentElement;
  if(!f||!f.getAttribute)return;
  e.preventDefault();
  var kind=f.getAttribute("data-form");
  var btn=f.querySelector('button[type="submit"]');
  if(btn)btn.disabled=true;
  try{
    if(kind==="login")await doLogin(f);
    else if(kind==="register")await doRegister(f);
    else if(kind==="claim")await doClaim(f);
    else if(kind==="edit-item")await doEditItem(f);
    else if(kind==="publish")await doPublish(f);
    else if(kind==="verify")await doVerify(f);
    else if(kind==="password")await doPassword(f);
    else if(kind==="invite")await doInvite(f);
    else if(kind==="admin-search")await adminSearch(valOf(f,"q"));
    else if(kind==="search"){HOME.q=valOf(f,"q");HOME.page=1;await loadList();}
    else if(kind==="report")await doReport(f);
  }catch(err){fail(err);}
  finally{if(btn)btn.disabled=false;}
});
document.addEventListener("change",function(e){
  var s=e.target;
  while(s&&s.getAttribute&&!s.getAttribute("data-filter"))s=s.parentElement;
  if(!s||!s.getAttribute)return;
  HOME[s.getAttribute("data-filter")]=s.value;
  HOME.page=1;
  loadList();
});
document.addEventListener("click",async function(e){
  var el=e.target;
  if(el&&el.classList&&el.classList.contains("modal")){closeModal();return;}
  var t=el;
  while(t&&t.getAttribute&&!t.getAttribute("data-act"))t=t.parentElement;
  if(!t||!t.getAttribute)return;
  var act=t.getAttribute("data-act");
  var pid=t.getAttribute("data-pid")||"";
  var id=t.getAttribute("data-id")||"";
  try{
    if(act==="login-tab"){LOGINTAB=t.getAttribute("data-tab")||"login";renderLogin();}
    else if(act==="home-type"){HOME.type=t.getAttribute("data-type")||"";HOME.page=1;await renderHome();}
    else if(act==="page"){var p=num(t.getAttribute("data-page"),1);if(p<1)return;HOME.page=p;await loadList();window.scrollTo(0,0);}
    else if(act==="my-tab"){MYTAB=t.getAttribute("data-tab")||"items";await renderMy();}
    else if(act==="edit-open"){DET.editing=true;renderDetailView();}
    else if(act==="edit-cancel"){DET.editing=false;renderDetailView();}
    else if(act==="item-delete")await doDelete(pid);
    else if(act==="report-open"){if(pid)DET.pid=pid;openModal(reportModalHtml());}
    else if(act==="claim-approve")await doApprove(id);
    else if(act==="claim-reject")await doReject(id);
    else if(act==="claim-cancel")await doCancel(id);
    else if(act==="notif-open")await doNotifOpen(id,t.getAttribute("data-link")||"",t);
    else if(act==="notif-all")await doNotifAll();
    else if(act==="admin-report")await doHandleReport(id,t.getAttribute("data-action")||"");
    else if(act==="admin-takedown")await doTakeDown(pid);
    else if(act==="logout")await doLogout();
    else if(act==="zoom")openModal('<div class="zoombar"><button type="button" class="btn sm" data-act="modal-close">关闭</button></div><img class="zoomimg" src="'+esc(t.getAttribute("data-src"))+'" alt="放大图片">');
    else if(act==="modal-close")closeModal();
  }catch(err){fail(err);}
});
function reportModalHtml(){
  return '<h3>举报物品</h3><p class="muted">请简要说明举报原因，管理员会尽快处理。</p><form data-form="report"><label class="flabel">举报原因<textarea class="input" name="reason" rows="3" maxlength="200" placeholder="例如：信息不实 / 涉嫌诈骗 / 内容不当"></textarea></label><div class="fbtns"><button class="btn danger" type="submit">提交举报</button><button class="btn" type="button" data-act="modal-close">取消</button></div></form>';
}

/* ---------------- 启动 ---------------- */
async function boot(){
  updateTopbar();
  try{
    var me=await api("GET","/api/auth/me",undefined,{silent:true});
    S.user=me.user;
    S.unread=num(me.unreadCount,0);
  }catch(e){S.user=null;}
  updateTopbar();
  if(S.user)connectStream();
  window.addEventListener("hashchange",function(){route();});
  await route();
}
boot();
})();
</script>
</body>
</html>)LF";

} // namespace webui_detail

inline const std::string& WebUiHtml() {
    static const std::string html = std::string(webui_detail::kHtml1) + webui_detail::kHtml2 + webui_detail::kHtml3 + webui_detail::kHtml4 + webui_detail::kHtml5 + webui_detail::kHtml6 + webui_detail::kHtml7 + webui_detail::kHtml8;
    return html;
}

} // namespace lf
