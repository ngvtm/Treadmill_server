// Tia nón 25° từ trụ nghiêng, nhìn từ trên xuống.
// Băng 75(dài, trục X) × 40(rộng, trục Y). Trụ sensor ở mép trên (y=0).
// Mỗi sensor có nón 25°, chiếu xuống: vệt nở theo khoảng cách tia đi.
function css(v){ return getComputedStyle(document.documentElement).getPropertyValue(v).trim(); }

const L=75, W=40, HALF=Math.tan(12.5*Math.PI/180); // 0.2217
const HEIGHT=4; // chiều cao trụ so mặt băng (cm) — nhỏ, tia gần ngang

// vệt bán kính tại khoảng cách ngang d (tia đi chéo, xấp xỉ ngang vì H nhỏ)
function radiusAt(d){ return HALF*Math.sqrt(d*d+HEIGHT*HEIGHT); }

// 6 sensor, cách 14cm, phủ 75cm: tâm từ 7,21,35,49,63,70.7 -> dùng 6 con
const N=6, SP=75/N; // 12.5cm
const centers=[]; for(let i=0;i<N;i++) centers.push(SP*(i+0.5));

function draw(){
  const PX=7,M=45,w=L*PX+M*2,h=W*PX+M*2;
  const X=cm=>M+cm*PX, Y=cm=>M+cm*PX;
  const belt=css('--belt'),axis=css('--axis'),sensor=css('--sensor'),
    beam=css('--beam'),good=css('--good'),bad=css('--critical'),muted=css('--muted');
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="tia nón">`;
  // băng
  s+=`<rect x="${X(0)}" y="${Y(0)}" width="${L*PX}" height="${W*PX}" fill="${belt}" stroke="${axis}" rx="4"/>`;
  // trụ sensor ở mép trên (dải mỏng)
  s+=`<rect x="${X(0)}" y="${Y(0)-6}" width="${L*PX}" height="6" fill="${css('--frame')}" rx="3"/>`;
  s+=`<text x="${X(L/2)}" y="${Y(0)-10}" text-anchor="middle" font-size="11" fill="${muted}">trụ sensor (mặt huyền nghiêng)</text>`;

  // nón từ mỗi sensor: 2 tia biên dọc theo 2 hướng 12.5°, từ sensor tới mép xa
  for(const cx of centers){
    // sensor tại (cx, 0), nón chiếu xuống: 2 tia tạo góc ±12.5° với phương xuống dưới (dọc -Y)
    // trên top-view: tia trái/phải theo X, mở theo Y
    const sx=X(cx), sy=Y(0);
    // chiều dài tia tới mép xa (y=40) — đủ dài
    const R=radiusAt(W);
    const xl=sx - Math.tan(12.5*Math.PI/180)*R*PX, xr=sx + Math.tan(12.5*Math.PI/180)*R*PX;
    const yb=Y(W);
    s+=`<polygon points="${sx},${sy} ${xl},${yb} ${xr},${yb}" fill="${beam}" fill-opacity="0.25"/>`;
    s+=`<line x1="${sx}" y1="${sy}" x2="${xl}" y2="${yb}" stroke="${beam}" stroke-opacity="0.5"/>`;
    s+=`<line x1="${sx}" y1="${sy}" x2="${xr}" y2="${yb}" stroke="${beam}" stroke-opacity="0.5"/>`;
    s+=`<circle cx="${sx}" cy="${sy}" r="4" fill="${sensor}"/>`;
    s+=`<text x="${sx}" y="${sy-10}" text-anchor="middle" font-size="10" fill="${sensor}">S${i+1}</text>`;
  }

  // vệt phủ tại mép xa (y=40): mỗi con 1 đoạn
  s+=`<line x1="${X(0)}" y1="${Y(W)}" x2="${X(L)}" y2="${Y(W)}" stroke="${good}" stroke-width="2"/>`;
  for(const cx of centers){
    const r=radiusAt(W)*PX;
    s+=`<line x1="${X(cx)-r}" y1="${Y(W)}" x2="${X(cx)+r}" y2="${Y(W)}" stroke="${good}" stroke-width="4"/>`;
  }
  s+=`<text x="${X(L/2)}" y="${Y(W)+18}" text-anchor="middle" font-size="10.5" fill="${good}">vệt tại mép xa (y=40cm): mỗi con ⌀${(2*radiusAt(W)).toFixed(1)}cm — phủ chồng, không khe</text>`;

  // khe mù ở mép gần (y nhỏ, vệt hẹp): tô vùng giữa 2 sensor tại y=5..15
  for(let i=0;i<centers.length-1;i++){
    const x1=centers[i], x2=centers[i+1], mid=(x1+x2)/2;
    // vệt tại y=10
    const r10=radiusAt(10)*PX, cx10=X(mid);
    // khe chỉ còn nếu 2 vệt không chạm nhau ở y=10
    const gap = (x2-x1)*PX - 2*radiusAt(10)*PX;
    if(gap>0){
      s+=`<rect x="${X(mid)-gap/2}" y="${Y(10)}" width="${gap}" height="${6*PX}" fill="${bad}" fill-opacity="0.5"/>`;
    }
  }
  s+=`<text x="${X(L/2)}" y="${Y(14)}" text-anchor="middle" font-size="9.5" fill="${bad}">khe mù ở mép gần (vệt hẹp) — nếu có</text>`;

  // nhãn kích thước
  s+=`<text x="${X(0)-6}" y="${Y(W/2)}" text-anchor="end" font-size="11" fill="${muted}">40cm</text>`;
  s+=`</svg>`; return s;
}
document.getElementById('svg').innerHTML=draw();
