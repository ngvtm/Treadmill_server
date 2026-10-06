// Mặt cắt ngang băng chạy (nhìn từ hông). Trục X = bề rộng 63cm, Y = chiều cao.
// Mỗi hình minh họa 1 cách lắp sensor + đường tia + vùng rủi ro.
const W = 63, H = 55, PX = 6, M = 30;
function css(v){ return getComputedStyle(document.documentElement).getPropertyValue(v).trim(); }

function frame(title){
  const w = W*PX + M*2, h = H*PX + M*2;
  const X = cm => M + cm*PX;
  const Y = cm => (M + H*PX) - cm*PX; // 0 = mặt băng, dương = lên trên
  return { w, h, X, Y, s:'' };
}

// vẽ nền băng + nhãn trục cho 1 hình
function drawBase(f){
  const {X,Y} = f;
  const belt=css('--belt'), axis=css('--axis'), muted=css('--muted');
  let s = `<svg viewBox="0 0 ${f.w} ${f.h}" role="img" aria-label="mặt cắt">`;
  // khối băng
  s += `<rect x="${X(0)}" y="${Y(0)}" width="${W*PX}" height="${8*PX}" fill="${belt}" stroke="${axis}"/>`;
  s += `<text x="${X(W/2)}" y="${Y(0)+18}" text-anchor="middle" font-size="10" fill="${muted}">mặt băng chạy — rộng 63 cm</text>`;
  return s;
}

// vẽ trẻ đứng (chân + thân) tại vị trí giữa băng
function drawChild(f, standing=true){
  const {X,Y} = f;
  const c = css('--child'); const cx = W*0.5;
  let s='';
  if (standing){
    // 2 chân
    s += `<rect x="${X(cx-6)}" y="${Y(22)}" width="${3*PX}" height="${22*PX}" rx="3" fill="${c}" fill-opacity="0.85"/>`;
    s += `<rect x="${X(cx+3)}" y="${Y(22)}" width="${3*PX}" height="${22*PX}" rx="3" fill="${c}" fill-opacity="0.85"/>`;
    // thân
    s += `<rect x="${X(cx-7)}" y="${Y(40)}" width="${11*PX}" height="${18*PX}" rx="5" fill="${c}" fill-opacity="0.55"/>`;
    s += `<text x="${X(cx)}" y="${Y(43)}" text-anchor="middle" font-size="9" fill="#fff">trẻ</text>`;
  }
  return s;
}
function beam(f, sx, sy, angleDeg, len, col){
  // vẽ nón tia 25° từ (sx,sy) theo hướng angleDeg (0=ngang phải, 90=lên)
  const {X,Y} = f;
  const half = 12.5*Math.PI/180, a = angleDeg*Math.PI/180;
  const p = (da)=>[sx+len*Math.cos(a+da), sy+len*Math.sin(a+da)];
  const [x1,y1]=p(half), [x2,y2]=p(-half);
  return `<polygon points="${X(sx)},${Y(sy)} ${X(x1)},${Y(y1)} ${X(x2)},${Y(y2)}" fill="${col}" fill-opacity="0.28"/>`;
}
function sensorDot(f, x, y){
  const {X,Y}=f; return `<circle cx="${X(x)}" cy="${Y(y)}" r="4.5" fill="${css('--sensor')}" stroke="${css('--surface-1')}" stroke-width="1.5"/>`;
}
function riskZone(f, x, y, w, h, label){
  const {X,Y}=f; const c=css('--critical');
  let s = `<rect x="${X(x)}" y="${Y(y)}" width="${w*PX}" height="${h*PX}" fill="${c}" fill-opacity="0.30" stroke="${c}" stroke-dasharray="3 2"/>`;
  if(label) s += `<text x="${X(x+w/2)}" y="${Y(y+h/2)}" text-anchor="middle" font-size="8.5" fill="${c}" font-weight="700">${label}</text>`;
  return s;
}

// PA1: nhô lên 1cm, chiếu ngang
function drawA1(){
  const f=frame(); let s=drawBase(f);
  s += drawChild(f);
  s += beam(f, 0, 1, 0, W, css('--beam'));          // tia ngang sát băng
  s += sensorDot(f, 0, 1);
  s += riskZone(f, 8, 0, 47, 2, 'nón quét trúng mặt băng → nhiễu nền');
  s += riskZone(f, -1, 0, 3, 5, '');
  s += `<text x="${f.X(1)}" y="${f.Y(7)}" font-size="8.5" fill="${css('--critical')}" font-weight="700">gờ nhô → vấp</text>`;
  s += `</svg>`; return s;
}
// PA2: âm rãnh, chiếu chếch lên
function drawA2(){
  const f=frame(); let s=drawBase(f);
  s += drawChild(f);
  // rãnh âm
  s += `<rect x="${f.X(-1)}" y="${f.Y(0)}" width="${5*PX}" height="${4*PX}" fill="${css('--frame')}" fill-opacity="0.5"/>`;
  s += beam(f, 1, -1, 8, W, css('--beam'));          // chếch lên nhẹ
  s += sensorDot(f, 1, -1);
  s += `<text x="${f.X(2)}" y="${f.Y(-2)}+0" font-size="8.5" fill="${css('--good')}" font-weight="700">mặt phẳng, không gờ</text>`;
  s += `</svg>`; return s;
}
// PA3: chiếu từ trên xuống
function drawA3(){
  const f=frame(); let s=drawBase(f);
  s += drawChild(f);
  // thanh ngang trên cao
  s += `<rect x="${f.X(0)}" y="${f.Y(52)}" width="${W*PX}" height="${2*PX}" fill="${css('--frame')}"/>`;
  for(const x of [10,25,40,55]){
    s += beam(f, x, 51, -90, 51, css('--beam'));     // chiếu xuống
    s += sensorDot(f, x, 51);
  }
  s += `<text x="${f.X(W/2)}" y="${f.Y(54)+2}" text-anchor="middle" font-size="9" fill="${css('--muted')}">thanh ngang phía trên</text>`;
  s += `</svg>`; return s;
}
// PA4: nâng 3-5cm, vát nghiêng
function drawA4(){
  const f=frame(); let s=drawBase(f);
  s += drawChild(f);
  s += beam(f, 0, 4, 6, W, css('--beam'));           // nâng cao, chếch nhẹ
  s += sensorDot(f, 0, 4);
  // mép dốc thoải
  s += `<polygon points="${f.X(-1)},${f.Y(0)} ${f.X(3)},${f.Y(0)} ${f.X(-1)},${f.Y(5)}" fill="${css('--frame')}" fill-opacity="0.5"/>`;
  s += `<text x="${f.X(4)}" y="${f.Y(5)}" font-size="8.5" fill="${css('--warning')}" font-weight="700">nâng ~4cm + mép dốc thoải</text>`;
  s += `</svg>`; return s;
}

const cards = [
  { fn:drawA1, title:'1. Nhô 1cm, chiếu ngang', tag:'bad', tagText:'KHÔNG nên',
    desc:'Đúng ý tưởng ban đầu của bạn.',
    pros:['Bắt bàn chân/mắt cá ổn định'],
    cons:['Nón 25° quét trúng mặt băng → nhiễu nền liên tục','Gờ cứng nhô lên = vật vấp ngã cho trẻ CP','Bàn chân trước che khuất con phía sau'] },
  { fn:drawA2, title:'2. Âm rãnh, chiếu chếch lên', tag:'ok', tagText:'Tốt',
    desc:'Giữ tia thấp nhưng bỏ gờ vấp.',
    pros:['Mặt trên phẳng lì với băng → không vấp','Tia chếch lên → thoát nhiễu mặt băng','Vẫn bắt chân tầm thấp'],
    cons:['Cần gia công rãnh âm','Vẫn cần hạ ngưỡng < thành đối diện'] },
  { fn:drawA3, title:'3. Chiếu từ trên xuống', tag:'ok', tagText:'Tốt nhất',
    desc:'Đổi hình học: đo khoảng cách tới đầu/thân.',
    pros:['Không vật cản nào trong tầm chân','Không nhiễu mặt băng song song','Ngã → khoảng cách vọt lên → phát hiện tự nhiên'],
    cons:['Cần khung phía trên băng','Tính lại số con theo đường kính spot'] },
  { fn:drawA4, title:'4. Nâng ~4cm + vát nghiêng', tag:'mid', tagText:'Tạm được',
    desc:'Nếu buộc lắp cạnh băng.',
    pros:['Thoát nhiễu mặt băng','Mép dốc thoải giảm nguy cơ vấp'],
    cons:['Vẫn có gờ (dù thoải)','Occlusion ngang vẫn còn'] },
];

const grid = document.getElementById('grid');
for(const c of cards){
  const el = document.createElement('div'); el.className='card';
  el.innerHTML = `<h2>${c.title} <span class="tag ${c.tag}">${c.tagText}</span></h2>`
    + `<p>${c.desc}</p>` + c.fn()
    + `<ul>` + c.pros.map(p=>`<li style="color:var(--good)">✓ ${p}</li>`).join('')
    + c.cons.map(p=>`<li style="color:var(--critical)">✗ ${p}</li>`).join('') + `</ul>`;
  grid.appendChild(el);
}
