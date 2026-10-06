// 3 hình chiếu kỹ thuật, vẽ CHÙM NÓN thật (không phải tia mảnh).
// Băng 98(dài) x 63(rộng), thanh chắn cao H=130cm, FoV 25° => bán kính vệt = H*tan(12.5°).
function css(v){ return getComputedStyle(document.documentElement).getPropertyValue(v).trim(); }
const H = 130, HALF = Math.tan(12.5*Math.PI/180); // 0.2217
const R = H*HALF;              // bán kính vệt trên mặt băng ~28.8cm
const L = 98, WID = 63;

// bố trí sensor: 1 hàng dọc theo TÂM chiều rộng, cách nhau dọc chiều dài.
// spacing chọn để vệt phủ kín — sẽ tính và hiển thị.
const SPACING = 40;            // cm giữa 2 sensor dọc chiều dài
const centers = [];
for(let x=SPACING/2; x<L; x+=SPACING) centers.push(x);   // vị trí dọc dài
const nSensors = centers.length;

// ---- 1. TOP VIEW ----
function drawTop(){
  const PX=6,M=40, w=L*PX+M*2, h=WID*PX+M*2;
  const X=cm=>M+cm*PX, Y=cm=>M+cm*PX;
  const belt=css('--belt'),axis=css('--axis'),good=css('--good'),sensor=css('--sensor'),muted=css('--muted');
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="top">`;
  s+=`<rect x="${X(0)}" y="${Y(0)}" width="${L*PX}" height="${WID*PX}" fill="${belt}" stroke="${axis}" rx="6"/>`;
  // vệt nón (ellipse tròn bán kính R) tại tâm chiều rộng
  for(const cx of centers){
    s+=`<circle cx="${X(cx)}" cy="${Y(WID/2)}" r="${R*PX}" fill="${good}" fill-opacity="0.22" stroke="${good}" stroke-opacity="0.5"/>`;
    s+=`<circle cx="${X(cx)}" cy="${Y(WID/2)}" r="3.5" fill="${sensor}"/>`;
  }
  // kích thước
  s+=`<text x="${X(L/2)}" y="${Y(WID)+22}" text-anchor="middle" font-size="11" fill="${muted}">dài 98cm — ${nSensors} sensor, vệt ⌀${(2*R).toFixed(0)}cm</text>`;
  s+=`<text x="${X(0)-6}" y="${Y(WID/2)}" text-anchor="end" font-size="11" fill="${muted}">63cm</text>`;
  // đánh dấu góc chưa phủ nếu có
  s+=`</svg>`; return s;
}

// ---- 2. SIDE VIEW (dọc chiều dài) ----
function drawSide(){
  const PX=5.5,M=40, HT=145, w=L*PX+M*2, h=HT*PX+M*2;
  const X=cm=>M+cm*PX, Y=cm=>(M+HT*PX)-cm*PX;
  const belt=css('--belt'),axis=css('--axis'),frame=css('--frame'),beam=css('--beam'),
    sensor=css('--sensor'),muted=css('--muted'),child=css('--child');
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="side">`;
  s+=`<rect x="${X(0)}" y="${Y(0)}" width="${L*PX}" height="${6*PX}" fill="${belt}" stroke="${axis}"/>`;
  s+=`<rect x="${X(0)}" y="${Y(H+8)}" width="${L*PX}" height="${5*PX}" fill="${frame}"/>`;   // thanh chắn
  for(const cx of centers){
    s+=`<circle cx="${X(cx)}" cy="${Y(H)}" r="3.5" fill="${sensor}"/>`;
    // nón xòe: từ sensor tới 2 mép vệt (cx-R, cx+R) ở mặt băng
    s+=`<polygon points="${X(cx)},${Y(H)} ${X(cx-R)},${Y(6)} ${X(cx+R)},${Y(6)}" fill="${beam}" fill-opacity="0.16" stroke="${beam}" stroke-opacity="0.3"/>`;
  }
  // trẻ
  s+=`<rect x="${X(L/2-6)}" y="${Y(6)}" width="${12*PX}" height="${55*PX}" rx="5" fill="${child}" fill-opacity="0.5"/>`;
  s+=`<text x="${X(L/2)}" y="${Y(70)}" text-anchor="middle" font-size="9" fill="#fff">trẻ</text>`;
  s+=`<text x="${X(L/2)}" y="${Y(0)+18}" text-anchor="middle" font-size="11" fill="${muted}">dài 98cm — các nón chồng nhau ở tầm thấp</text>`;
  s+=`<text x="${X(L)+6}" y="${Y(H)}" font-size="10" fill="${muted}">130cm</text>`;
  s+=`</svg>`; return s;
}

// ---- 3. FRONT VIEW (ngang bề rộng) ----
function drawFront(){
  const PX=6,M=45, HT=145, w=WID*PX+M*2+120, h=HT*PX+M*2;
  const X=cm=>M+cm*PX, Y=cm=>(M+HT*PX)-cm*PX;
  const belt=css('--belt'),axis=css('--axis'),frame=css('--frame'),beam=css('--beam'),
    sensor=css('--sensor'),muted=css('--muted'),child=css('--child'),bad=css('--critical');
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="front">`;
  s+=`<rect x="${X(0)}" y="${Y(0)}" width="${WID*PX}" height="${6*PX}" fill="${belt}" stroke="${axis}"/>`;
  s+=`<rect x="${X(0)}" y="${Y(H+8)}" width="${WID*PX}" height="${5*PX}" fill="${frame}"/>`;
  // 1 sensor ở tâm chiều rộng, nón xòe ngang tới cx±R
  const cx=WID/2;
  s+=`<circle cx="${X(cx)}" cy="${Y(H)}" r="4" fill="${sensor}"/>`;
  s+=`<polygon points="${X(cx)},${Y(H)} ${X(cx-R)},${Y(6)} ${X(cx+R)},${Y(6)}" fill="${beam}" fill-opacity="0.18" stroke="${beam}" stroke-opacity="0.4"/>`;
  // vệt phủ mặt băng cx±R vs mép 0..63
  const left=Math.max(0,cx-R), right=Math.min(WID,cx+R);
  s+=`<line x1="${X(left)}" y1="${Y(6)}" x2="${X(right)}" y2="${Y(6)}" stroke="${css('--good')}" stroke-width="3"/>`;
  // phần mép chưa phủ (nếu vệt < 63) — ở đây R=28.8 => vệt 57.6 < 63 => 2 mép hở ~2.7cm mỗi bên
  if(cx-R>0){ s+=`<rect x="${X(0)}" y="${Y(6)}" width="${(cx-R)*PX}" height="${6*PX}" fill="${bad}" fill-opacity="0.35"/>`; }
  if(cx+R<WID){ s+=`<rect x="${X(cx+R)}" y="${Y(6)}" width="${(WID-(cx+R))*PX}" height="${6*PX}" fill="${bad}" fill-opacity="0.35"/>`; }
  s+=`<text x="${X(cx)}" y="${Y(0)+18}" text-anchor="middle" font-size="11" fill="${muted}">rộng 63cm — vệt ⌀${(2*R).toFixed(0)}cm</text>`;
  s+=`<text x="${X(WID)+10}" y="${Y(H)}" font-size="10" fill="${muted}">130cm</text>`;
  if(cx-R>0) s+=`<text x="${X(0)+4}" y="${Y(12)}" font-size="8.5" fill="${bad}" font-weight="700">mép hở ~${(cx-R).toFixed(1)}cm</text>`;
  s+=`</svg>`; return s;
}

document.getElementById('top').innerHTML=drawTop();
document.getElementById('side').innerHTML=drawSide();
document.getElementById('front').innerHTML=drawFront();

// kết luận tính toán
const gapEdge=(WID/2-R);
document.getElementById('verdict').innerHTML =
  `<b>Kết luận hình học:</b> nón 25° từ 130cm cho vệt đường kính ~${(2*R).toFixed(0)}cm. `
  + `Theo bề rộng 63cm: 1 hàng giữa để hở mỗi mép ~${gapEdge.toFixed(1)}cm (${gapEdge>0?'CÓ hở nhẹ ở 2 mép':'phủ kín'}). `
  + `Theo chiều dài 98cm: ${nSensors} sensor cách ${SPACING}cm, vệt ⌀${(2*R).toFixed(0)}cm > khoảng cách nên phủ chồng dọc. `
  + `→ Chiếu xuống cần ÍT sensor hơn hẳn rào ngang, NHƯNG vệt tròn để hở 2 góc/mép — cần nghiêng sensor vào trong hoặc hạ thanh chắn thấp hơn để vệt to hơn tương đối.`;
