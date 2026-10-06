// Minh họa isometric 3D. Tự tính bounding box để không bao giờ bị cắt (crop).
function css(v){ return getComputedStyle(document.documentElement).getPropertyValue(v).trim(); }
const ISO = Math.cos(Math.PI/6);

// mỗi bản vẽ có 1 tracker gom điểm để tính viewBox
function newScene(k){ return { k, pts:[], body:'' }; }
function P(sc,x,y,z){
  const sx=(x-y)*ISO*sc.k, sy=(x+y)*0.5*sc.k - z*sc.k;
  sc.pts.push([sx,sy]); return [sx,sy];
}
function shade(hex,f){
  const n=parseInt((hex||'#888').replace('#',''),16);
  let r=(n>>16)&255,g=(n>>8)&255,b=n&255;
  return `rgb(${Math.min(255,r*f)|0},${Math.min(255,g*f)|0},${Math.min(255,b*f)|0})`;
}
function poly(sc,pts3,fill,op){
  const p=pts3.map(q=>P(sc,q[0],q[1],q[2]));
  const d=p.map(q=>`${q[0].toFixed(1)},${q[1].toFixed(1)}`).join(' ');
  sc.body+=`<polygon points="${d}" fill="${fill}" fill-opacity="${op??1}" stroke="${shade(typeof fill==='string'&&fill[0]==='#'?fill:'#555',0.6)}" stroke-width="0.8" stroke-linejoin="round"/>`;
}
function box(sc,x,y,z,dx,dy,dz,base){
  const top=shade(base,1.18),left=shade(base,0.72),right=shade(base,0.9);
  poly(sc,[[x,y+dy,z],[x+dx,y+dy,z],[x+dx,y+dy,z+dz],[x,y+dy,z+dz]],right);   // mặt phải
  poly(sc,[[x+dx,y,z],[x+dx,y+dy,z],[x+dx,y+dy,z+dz],[x+dx,y,z+dz]],left);    // mặt trước
  poly(sc,[[x,y,z+dz],[x+dx,y,z+dz],[x+dx,y+dy,z+dz],[x,y+dy,z+dz]],top);     // mặt trên
}
function line3(sc,a,b,col,wd,dash){
  const p=P(sc,a[0],a[1],a[2]),q=P(sc,b[0],b[1],b[2]);
  sc.body+=`<line x1="${p[0].toFixed(1)}" y1="${p[1].toFixed(1)}" x2="${q[0].toFixed(1)}" y2="${q[1].toFixed(1)}" stroke="${col}" stroke-width="${wd||1}" ${dash?`stroke-dasharray="${dash}"`:''}/>`;
  return q;
}
function dot3(sc,x,y,z,r,col){ const p=P(sc,x,y,z); sc.body+=`<circle cx="${p[0].toFixed(1)}" cy="${p[1].toFixed(1)}" r="${r}" fill="${col}"/>`; return p; }
// nhãn: đặt tại điểm 3D + lệch màn hình (dx,dy). Track cả điểm nhãn để không bị cắt.
function label(sc,x,y,z,dx,dy,text,col,anchor){
  const p=P(sc,x,y,z); const lx=p[0]+dx, ly=p[1]+dy;
  sc.pts.push([lx-text.length*3.2,ly],[lx+text.length*6.2,ly]); // ước lượng bề rộng chữ
  sc.body+=`<line x1="${p[0].toFixed(1)}" y1="${p[1].toFixed(1)}" x2="${lx.toFixed(1)}" y2="${ly.toFixed(1)}" stroke="${col}" stroke-width="0.8"/>`;
  sc.body+=`<text x="${lx.toFixed(1)}" y="${(ly-3).toFixed(1)}" font-size="12" fill="${col}" text-anchor="${anchor||'start'}">${text}</text>`;
}
function title(sc,x,y,z,text){
  const p=P(sc,x,y,z); sc.pts.push([p[0]-text.length*3.5,p[1]],[p[0]+text.length*3.5,p[1]]);
  sc.body+=`<text x="${p[0].toFixed(1)}" y="${(p[1]-4).toFixed(1)}" text-anchor="middle" font-size="12.5" fill="${css('--muted')}">${text}</text>`;
}
// bọc thành SVG với viewBox tự tính từ mọi điểm + padding
function finish(sc){
  const xs=sc.pts.map(p=>p[0]), ys=sc.pts.map(p=>p[1]);
  const pad=24;
  const minx=Math.min(...xs)-pad, miny=Math.min(...ys)-pad;
  const w=Math.max(...xs)-minx+pad, h=Math.max(...ys)-miny+pad;
  return `<svg viewBox="${minx.toFixed(1)} ${miny.toFixed(1)} ${w.toFixed(1)} ${h.toFixed(1)}" role="img">${sc.body}</svg>`;
}

// ===== A. Tổng thể =====
function drawOverview(){
  const sc=newScene(2.4);
  const belt=css('--belt'),frame=css('--frame'),sensor=css('--sensor'),
    child='#4a3aa7',beam=css('--beam'),good=css('--good'),warn=css('--warning');
  box(sc,0,0,0, 98,63,6, belt);                       // sàn băng
  for(const c of [[-6,-6],[-6,63],[98,-6],[98,63]]) box(sc,c[0],c[1],0,6,6,130,frame); // 4 cột
  box(sc,-6,-6,130, 110,8,8, frame);                  // thanh chắn trên
  const sxs=[6,19,32,45,58,71,84,94];
  for(const cx of sxs){
    box(sc,cx-2,-4,124, 4,4,4, sensor);
    line3(sc,[cx,-2,124],[cx,-2,6],beam,1,'3 3');
  }
  box(sc,40,26,6, 12,12,70, child);                   // thân trẻ
  const hc=dot3(sc,46,32,88,16,shade(child,1.1));
  sc.body+=`<text x="${hc[0]}" y="${(hc[1]-22).toFixed(1)}" text-anchor="middle" font-size="12" fill="${css('--muted')}">trẻ ~100cm</text>`;
  label(sc,94,-2,65, 40,-10,'baseline: tia tới mặt băng ~130cm',good);
  label(sc,46,32,55, 60,30,'có trẻ → khoảng cách rút còn ~30cm',warn);
  title(sc,50,-6,150,'Thanh chắn trên — 8 sensor chiếu xuống');
  return finish(sc);
}

// ===== B. Giá đỡ 1 sensor =====
function drawMount(){
  const sc=newScene(10);
  const frame=css('--frame'),sensor=css('--sensor'),good=css('--good'),bolt=css('--text-secondary');
  box(sc,0,0,20, 34,12,6, frame);                     // đoạn thanh chắn
  box(sc,9,1,11, 16,10,9, shade(sensor,0.5));         // thân giá đỡ (tối)
  box(sc,11,2,12, 12,8,6, sensor);                    // sensor
  box(sc,15,4,17, 4,4,3, good);                       // chốt định vị
  for(const bx of [5,29]){                            // 2 vít trên mặt thanh
    const p=dot3(sc,bx,6,26,5.5,bolt);
    sc.body+=`<line x1="${(p[0]-4).toFixed(1)}" y1="${p[1].toFixed(1)}" x2="${(p[0]+4).toFixed(1)}" y2="${p[1].toFixed(1)}" stroke="${css('--surface-1')}" stroke-width="1.5"/>`;
  }
  line3(sc,[17,6,12],[17,6,0],beamCol(),2.5);         // tia chiếu xuống (ngắn)
  label(sc,5,6,26, -70,-30,'2 vít + keo khóa ren',bolt,'end');
  label(sc,17,5,20, 70,-6,'chốt định vị (khóa góc xoay)',good);
  label(sc,17,6,14, 70,40,'giá đỡ ôm thân sensor',sensor);
  label(sc,17,6,0, -20,40,'tia chiếu xuống',beamCol(),'end');
  return finish(sc);
}
function beamCol(){ return css('--beam'); }

// ===== C. Phát hiện lệch =====
function drawBaseline(){
  const sc=newScene(2.4);
  const belt=css('--belt'),frame=css('--frame'),sensor=css('--sensor'),good=css('--good'),bad=css('--critical');
  box(sc,0,0,0, 98,63,6, belt);
  box(sc,-6,-6,110, 110,8,8, frame);
  const sxs=[8,25,42,59,76,90];
  sxs.forEach((cx,i)=>{
    const isBad=(i===3);
    box(sc,cx-2,-4,104, 4,4,4, isBad?bad:sensor);
    if(isBad){
      line3(sc,[cx,-2,104],[cx+24,-2,6],bad,2.2);
      label(sc,cx+24,-2,6, 10,26,'LỆCH: d ≠ baseline → báo lỗi + fail-closed',bad);
    } else {
      line3(sc,[cx,-2,104],[cx,-2,6],good,1.4,'3 3');
    }
  });
  label(sc,8,-2,58, -60,-6,'5 con: d = baseline ✓',good,'end');
  title(sc,50,-6,124,'Băng TRỐNG — hệ tự kiểm mỗi con so baseline');
  return finish(sc);
}

document.getElementById('svgA').innerHTML = drawOverview();
document.getElementById('svgB').innerHTML = drawMount();
document.getElementById('svgC').innerHTML = drawBaseline();
