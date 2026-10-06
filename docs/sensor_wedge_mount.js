// Gờ mount cho GY-VL53L0XV2. Mặt lắp PCB gần THẲNG ĐỨNG.
//
// HÌNH HỌC (kiểm lại cho đúng):
//   trục quang VL53L0X = pháp tuyến mặt PCB.
//   Muốn trục quang ngẩng AXIS=12.5 độ so NGANG
//   => mặt PCB phải hợp với mặt NGANG một góc (90-12.5)=77.5 độ
//   => tức mặt PCB gần THẲNG ĐỨNG, chỉ ngả ra sau 12.5 độ khỏi phương đứng.
//   Tia thấp nhất = AXIS - HALF = 12.5 - 12.5 = 0 độ = song song mặt băng. ✓
//
//   Kiểm tra ngược: mặt gần NẰM NGANG thì pháp tuyến gần THẲNG ĐỨNG
//   => tia bắn lên trần. Đó là lỗi của bản vẽ trước.
function css(v){ return getComputedStyle(document.documentElement).getPropertyValue(v).trim(); }

// ---- module GY-VL53L0XV2 ----
const PCB_L = 25.0, PCB_W = 10.6, PCB_T = 1.6;
const HOLE_PITCH = 19.0, HOLE_D = 2.2;
const CHIP_L = 4.4, CHIP_W = 2.4;
const HDR_N = 6, HDR_PITCH = 2.54;

// ---- quang học ----
const SEAT_FROM_HORIZ = 77.5;              // độ, mặt lắp so mặt NGANG (gần đứng)
const SEAT_LEAN       = 90 - SEAT_FROM_HORIZ; // = 12.5, ngả ra sau khỏi phương đứng
const AXIS            = 90 - SEAT_FROM_HORIZ; // = 12.5, trục quang so ngang
const HALF            = 12.5;
const RAY_LOW         = AXIS - HALF;       // = 0  -> song song mặt băng
const RAY_HIGH        = AXIS + HALF;       // = 25

// ---- băng & rail ----
const BELT_W = 650, BELT_L = 800;
const SEG_L = 200, SENS_PER_SEG = 2, PITCH = 100;
const WALL = 3.0;
const H_SENS = 22;                          // mm, tâm quang so mặt băng
const THRESH = 700;

const rad = d => d*Math.PI/180;
const slant = d => d/Math.cos(rad(AXIS));

// ---- thân nêm (mặt cắt trong mp y-z; y hướng ra băng, z lên) ----
const BODY_W = 34;                          // rộng đáy
const SEAT_H = 26;                          // chiều dài mặt lắp
const FOOT   = 5;                           // chân đế dưới mặt lắp
// mặt lắp đi từ P1 (dưới, gần băng) lên P2 (trên, lùi ra sau) — ngả 12.5 khỏi đứng
const SEAT_DY = -SEAT_H*Math.sin(rad(SEAT_LEAN));  // âm: lùi ra xa băng
const SEAT_DZ =  SEAT_H*Math.cos(rad(SEAT_LEAN));
const P1 = [BODY_W, FOOT];
const P2 = [BODY_W + SEAT_DY, FOOT + SEAT_DZ];
const BODY_H = P2[1] + 4;
const WIN_W = 12, WIN_H = 12;
// tâm quang tại trung điểm mặt lắp
const MY = (P1[0]+P2[0])/2, MZ = (P1[1]+P2[1])/2;

// ---------- so sánh góc ----------
function drawAngles(){
  const muted=css('--muted'),txt=css('--text-secondary'),beam=css('--beam'),
        bad=css('--critical'),good=css('--good'),pcb=css('--pcb'),fa=css('--face-a'),ax=css('--axis');
  const w=880,h=290;
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="so sánh góc">`;

  function panel(ox, seatFromHoriz, ok){
    const bx=ox+90, by=225;
    const axisAng = 90 - seatFromHoriz;
    const lowAng  = axisAng - HALF;
    const col = ok?good:bad;
    s+=`<line x1="${ox+14}" y1="${by}" x2="${ox+410}" y2="${by}" stroke="${ax}" stroke-width="2.2"/>`;
    s+=`<text x="${ox+16}" y="${by+15}" font-size="8.5" fill="${muted}">mặt băng</text>`;

    // mặt lắp: hợp seatFromHoriz với ngang, đi lên và LÙI (-x) khi seat gần đứng
    const len=64, a=rad(seatFromHoriz);
    const ex=bx-len*Math.cos(a), ey=by-28-len*Math.sin(a);
    s+=`<line x1="${bx}" y1="${by-28}" x2="${ex}" y2="${ey}" stroke="${fa}" stroke-width="10" stroke-linecap="round"/>`;
    // PCB áp mặt đó
    const pm=0.5, pcx=bx+(ex-bx)*pm, pcy=(by-28)+(ey-(by-28))*pm;
    s+=`<rect x="${pcx-4}" y="${pcy-15}" width="8" height="30" fill="${pcb}"
         transform="rotate(${-(90-seatFromHoriz)} ${pcx} ${pcy})"/>`;
    // nón quanh trục axisAng
    const ol=155;
    s+=`<polygon points="${pcx},${pcy} ${pcx+ol*Math.cos(rad(lowAng))},${pcy-ol*Math.sin(rad(lowAng))} ${pcx+ol*Math.cos(rad(axisAng+HALF))},${pcy-ol*Math.sin(rad(axisAng+HALF))}"
         fill="${beam}" fill-opacity="0.17" stroke="${beam}" stroke-opacity="0.3"/>`;
    s+=`<line x1="${pcx}" y1="${pcy}" x2="${pcx+ol*Math.cos(rad(lowAng))}" y2="${pcy-ol*Math.sin(rad(lowAng))}"
         stroke="${col}" stroke-width="2.6"/>`;
    s+=`<text x="${pcx+ol*Math.cos(rad(lowAng))+6}" y="${pcy-ol*Math.sin(rad(lowAng))+4}" font-size="9.5" fill="${col}" font-weight="700">tia thấp ${lowAng.toFixed(1)}°</text>`;
    s+=`<line x1="${pcx}" y1="${pcy}" x2="${pcx+ol*Math.cos(rad(axisAng))}" y2="${pcy-ol*Math.sin(rad(axisAng))}"
         stroke="${beam}" stroke-width="1.6" stroke-dasharray="5 4"/>`;
    s+=`<text x="${pcx+ol*Math.cos(rad(axisAng))+6}" y="${pcy-ol*Math.sin(rad(axisAng))-7}" font-size="9" fill="${beam}">trục ${axisAng.toFixed(1)}°</text>`;

    s+=`<text x="${ox+14}" y="26" font-size="11.5" fill="${col}" font-weight="700">mặt lắp ${seatFromHoriz}° so NGANG</text>`;
    s+=`<text x="${ox+14}" y="44" font-size="9.5" fill="${muted}">${seatFromHoriz>45?'gần THẲNG ĐỨNG':'gần NẰM NGANG'} · trục quang ${axisAng.toFixed(1)}° · tia thấp ${lowAng.toFixed(1)}°</text>`;
    s+=`<text x="${ox+14}" y="${by+38}" font-size="10" fill="${col}" font-weight="600">${ok?'✓ tia thấp SONG SONG mặt băng':'✗ tia bắn lên trần'}</text>`;
  }
  panel(10, 12.5, false);
  panel(450, 77.5, true);
  s+=`</svg>`;
  return s;
}

// ---------- A. đoạn rail 200mm ----------
function drawSegment(){
  const fa=css('--face-a'),fb=css('--face-b'),fc=css('--face-c'),cav=css('--cav'),
        ax=css('--axis'),muted=css('--muted'),txt=css('--text-secondary'),
        beam=css('--beam'),good=css('--good');
  const S=2.7;
  const PX=(x,y,z)=> 150 + x*S*0.88 + y*S*0.44;
  const PY=(x,y,z)=> 330 - x*S*0.19 + y*S*0.31 - z*S*0.95;
  const w=880,h=420;
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="đoạn rail">`;

  // biên dạng: P0 đáy sau, Pa đáy trước, P1 chân mặt lắp, P2 đỉnh mặt lắp, P3 đỉnh sau
  const prof=[[0,0],[BODY_W,0],P1,P2,[0,BODY_H]];
  function face(i0,i1,fill){
    const p=[[0,prof[i0][0],prof[i0][1]],[SEG_L,prof[i0][0],prof[i0][1]],
             [SEG_L,prof[i1][0],prof[i1][1]],[0,prof[i1][0],prof[i1][1]]]
      .map(q=>`${PX(q[0],q[1],q[2])},${PY(q[0],q[1],q[2])}`).join(' ');
    return `<polygon points="${p}" fill="${fill}" stroke="${ax}" stroke-width="0.8"/>`;
  }
  s+=face(0,1,fc);   // đáy
  s+=face(1,2,fb);   // chân đế trước
  s+=face(2,3,fa);   // MẶT LẮP (gần đứng)
  s+=face(3,4,fb);   // mặt trên
  s+=face(4,0,fc);   // mặt sau
  const cap=prof.map(p=>`${PX(0,p[0],p[1])},${PY(0,p[0],p[1])}`).join(' ');
  s+=`<polygon points="${cap}" fill="${fb}" stroke="${ax}" stroke-width="1.2"/>`;
  const t=WALL;
  const inner=[[t,t],[BODY_W-t,t],[P1[0]-t,P1[1]+t],[P2[0]-t,P2[1]-t],[t,BODY_H-t]]
    .map(p=>`${PX(0,p[0],p[1])},${PY(0,p[0],p[1])}`).join(' ');
  s+=`<polygon points="${inner}" fill="${cav}" fill-opacity="0.6" stroke="${ax}" stroke-opacity="0.5" stroke-dasharray="3 2"/>`;
  s+=`<text x="${PX(0,BODY_W*0.3,BODY_H*0.45)-56}" y="${PY(0,BODY_W*0.3,BODY_H*0.45)}" font-size="9" fill="${muted}">khoang dây</text>`;

  // 2 trạm sensor
  [PITCH/2, PITCH/2+PITCH].forEach((sxp,i)=>{
    // cửa sổ nằm trên mặt lắp, tâm tại (MY,MZ); hướng dọc mặt lắp
    const uy=-Math.sin(rad(SEAT_LEAN)), uz=Math.cos(rad(SEAT_LEAN)); // vector dọc mặt lắp
    const hw=WIN_H/2;
    const q=[[sxp-WIN_W/2,MY+uy*hw,MZ+uz*hw],[sxp+WIN_W/2,MY+uy*hw,MZ+uz*hw],
             [sxp+WIN_W/2,MY-uy*hw,MZ-uz*hw],[sxp-WIN_W/2,MY-uy*hw,MZ-uz*hw]]
      .map(p=>`${PX(p[0],p[1],p[2])},${PY(p[0],p[1],p[2])}`).join(' ');
    s+=`<polygon points="${q}" fill="${cav}" stroke="${good}" stroke-width="1.5"/>`;
    // tia thấp nhất song song + trục 12.5
    s+=`<line x1="${PX(sxp,MY,MZ)}" y1="${PY(sxp,MY,MZ)}" x2="${PX(sxp,MY+100,MZ)}" y2="${PY(sxp,MY+100,MZ)}"
         stroke="${good}" stroke-width="2.2"/>`;
    s+=`<line x1="${PX(sxp,MY,MZ)}" y1="${PY(sxp,MY,MZ)}" x2="${PX(sxp,MY+100,MZ+100*Math.tan(rad(AXIS)))}" y2="${PY(sxp,MY+100,MZ+100*Math.tan(rad(AXIS)))}"
         stroke="${beam}" stroke-width="1.5" stroke-dasharray="5 4"/>`;
    s+=`<text x="${PX(sxp,MY,MZ)-6}" y="${PY(sxp,MY,MZ)-16}" font-size="10" fill="${txt}" font-weight="700">S${i}</text>`;
    [-HOLE_PITCH/2,HOLE_PITCH/2].forEach(dx=>{
      s+=`<circle cx="${PX(sxp+dx,MY,MZ)}" cy="${PY(sxp+dx,MY,MZ)}" r="2" fill="none" stroke="${ax}" stroke-width="1.1"/>`;
    });
  });
  s+=`<text x="${PX(SEG_L*0.5,BODY_W+14,0)}" y="${PY(SEG_L*0.5,BODY_W+14,0)+16}" font-size="10" fill="${muted}">${SEG_L}mm — vừa bàn in 220×220</text>`;
  s+=`<text x="20" y="24" font-size="12" fill="${txt}" font-weight="700">1 đoạn rail ${SEG_L}mm, ${SENS_PER_SEG} sensor — in 4 đoạn cho băng 800mm</text>`;
  s+=`<text x="20" y="42" font-size="10" fill="${good}">Mặt lắp gần thẳng đứng · tia xanh = tia thấp nhất, song song mặt băng ở z=${H_SENS}mm</text>`;
  s+=`</svg>`;
  return s;
}

// ---------- B. trạm sensor ----------
function drawStation(){
  const fa=css('--face-a'),cav=css('--cav'),ax=css('--axis'),muted=css('--muted'),
        txt=css('--text-secondary'),pcb=css('--pcb'),good=css('--good'),
        sensor=css('--sensor'),beam=css('--beam'),warn=css('--warning');
  // nhìn vuông góc mặt lắp: dựng hệ (u = dọc rail, v = dọc mặt lắp)
  const S=6.2, cx=330, cy=210;
  const U=(u,v)=> cx + u*S;
  const V=(u,v)=> cy - v*S;
  const w=880,h=380;
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="trạm sensor">`;

  // mảnh mặt lắp
  s+=`<rect x="${U(-22)}" y="${V(13)}" width="${44*S}" height="${26*S}" fill="${fa}" stroke="${ax}" stroke-width="1.3" rx="2"/>`;
  // hốc chìm PCB
  s+=`<rect x="${U(-PCB_L/2)}" y="${V(PCB_W/2)}" width="${PCB_L*S}" height="${PCB_W*S}"
       fill="${cav}" fill-opacity="0.28" stroke="${ax}" stroke-dasharray="3 2"/>`;
  s+=`<text x="${U(-PCB_L/2)}" y="${V(PCB_W/2)-6}" font-size="9" fill="${muted}">hốc chìm ${PCB_L}×${PCB_W}×${PCB_T}mm (+0.3 dung sai)</text>`;
  // cửa sổ quang
  s+=`<rect x="${U(-WIN_W/2)}" y="${V(WIN_H/2)}" width="${WIN_W*S}" height="${WIN_H*S}"
       fill="${cav}" stroke="${good}" stroke-width="1.8" rx="1.5"/>`;
  s+=`<text x="${U(WIN_W/2)+8}" y="${V(0)}" font-size="9.5" fill="${good}" font-weight="700">cửa sổ ${WIN_W}×${WIN_H}mm</text>`;
  // 2 lỗ M2
  [-HOLE_PITCH/2,HOLE_PITCH/2].forEach(dx=>{
    s+=`<circle cx="${U(dx)}" cy="${V(0)}" r="${HOLE_D*S/2}" fill="none" stroke="${ax}" stroke-width="1.4"/>`;
  });
  s+=`<line x1="${U(-HOLE_PITCH/2)}" y1="${V(-8)}" x2="${U(HOLE_PITCH/2)}" y2="${V(-8)}" stroke="${muted}"/>`;
  s+=`<text x="${U(0)}" y="${V(-10)}" text-anchor="middle" font-size="9" fill="${muted}">M2 ×2, pitch ${HOLE_PITCH}mm</text>`;

  // PCB tách rời bên trái
  const ox=-52;
  s+=`<rect x="${U(ox-PCB_L/2)}" y="${V(PCB_W/2)}" width="${PCB_L*S}" height="${PCB_W*S}"
       fill="${pcb}" fill-opacity="0.85" stroke="${ax}" rx="1.5"/>`;
  [-PCB_L/2+2.5, PCB_L/2-2.5].forEach(dx=>{
    s+=`<circle cx="${U(ox+dx)}" cy="${V(0)}" r="${2.4*S/2}" fill="none" stroke="#fff" stroke-opacity="0.75" stroke-width="1.3"/>`;
  });
  s+=`<rect x="${U(ox-CHIP_L/2)}" y="${V(CHIP_W/2)}" width="${CHIP_L*S}" height="${CHIP_W*S}"
       fill="#15171b" stroke="${sensor}" stroke-width="1.4"/>`;
  s+=`<text x="${U(ox)}" y="${V(PCB_W/2)-6}" text-anchor="middle" font-size="9" fill="${sensor}" font-weight="700">VL53L0X</text>`;
  const names=['VIN','GND','SCL','SDA','GPIO1','XSHUT'];
  for(let i=0;i<HDR_N;i++){
    const px=ox-((HDR_N-1)/2)*HDR_PITCH + i*HDR_PITCH;
    s+=`<circle cx="${U(px)}" cy="${V(-PCB_W/2+1.6)}" r="${1.9}" fill="#e8b84b" stroke="#000" stroke-opacity="0.4"/>`;
  }
  s+=`<text x="${U(ox)}" y="${V(-PCB_W/2)+22}" text-anchor="middle" font-size="8.5" fill="${muted}">${names.join(' · ')}</text>`;
  s+=`<text x="${U(ox)}" y="${V(-PCB_W/2)+35}" text-anchor="middle" font-size="8.5" fill="${warn}">hàn dây trực tiếp gọn hơn cắm header</text>`;

  // mũi tên lắp
  s+=`<path d="M ${U(ox+PCB_L/2+3)} ${V(0)} L ${U(-PCB_L/2-4)} ${V(0)}"
       stroke="${good}" stroke-width="2" stroke-dasharray="4 3" marker-end="url(#ah3)"/>`;
  s+=`<defs><marker id="ah3" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto">
       <path d="M0,0 L6,3 L0,6 Z" fill="${good}"/></marker></defs>`;
  s+=`<text x="${U((ox+PCB_L/2-PCB_L/2)/2)-40}" y="${V(0)-8}" font-size="9.5" fill="${good}" font-weight="700">lắp từ mặt trong</text>`;

  s+=`<text x="20" y="24" font-size="12" fill="${txt}" font-weight="700">1 trạm sensor — nhìn vuông góc mặt lắp</text>`;
  s+=`<text x="20" y="42" font-size="10" fill="${muted}">PCB áp mặt trong, chip nhìn thẳng qua cửa sổ → không vật liệu trong đường quang</text>`;
  s+=`</svg>`;
  return s;
}

// ---------- C. mặt cắt ----------
function drawSection(){
  const fa=css('--face-a'),cav=css('--cav'),ax=css('--axis'),muted=css('--muted'),
        txt=css('--text-secondary'),pcb=css('--pcb'),beam=css('--beam'),good=css('--good'),
        sensor=css('--sensor');
  const P=5.0, M=56;
  const w=700, h=330;
  // đặt sao cho tâm quang ở đúng z=H_SENS so mặt băng
  const zOff = H_SENS - MZ;   // dịch cả mount lên/xuống để MZ -> H_SENS
  const X=mm=>M+70+mm*P, Y=mm=>h-M-mm*P;
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="mặt cắt">`;

  // mặt băng z=0
  s+=`<line x1="${X(-18)}" y1="${Y(0)}" x2="${X(108)}" y2="${Y(0)}" stroke="${ax}" stroke-width="2.4"/>`;
  s+=`<text x="${X(76)}" y="${Y(-6)}" font-size="9" fill="${muted}">mặt băng</text>`;

  const prof=[[0,0],[BODY_W,0],P1,P2,[0,BODY_H]].map(p=>[p[0],p[1]+zOff]);
  s+=`<polygon points="${prof.map(p=>`${X(p[0])},${Y(p[1])}`).join(' ')}"
       fill="${fa}" stroke="${ax}" stroke-width="1.7"/>`;
  const t=WALL;
  const inner=[[t,t],[BODY_W-t,t],[P1[0]-t,P1[1]+t],[P2[0]-t,P2[1]-t],[t,BODY_H-t]].map(p=>[p[0],p[1]+zOff]);
  s+=`<polygon points="${inner.map(p=>`${X(p[0])},${Y(p[1])}`).join(' ')}"
       fill="${cav}" fill-opacity="0.55" stroke="${ax}" stroke-dasharray="3 2" stroke-opacity="0.6"/>`;
  s+=`<text x="${X(7)}" y="${Y(BODY_H*0.45+zOff)}" font-size="8.5" fill="${muted}">khoang dây</text>`;

  const my=MY, mz=H_SENS;
  // PCB áp mặt trong, gần thẳng đứng (ngả 12.5 khỏi đứng)
  s+=`<g transform="rotate(${SEAT_LEAN} ${X(my)} ${Y(mz)})">
       <rect x="${X(my)-PCB_T*P*1.6}" y="${Y(mz)-PCB_W*P/2}" width="${PCB_T*P*1.8}" height="${PCB_W*P}" fill="${pcb}" stroke="${ax}"/>
       </g>`;
  s+=`<text x="${X(my)-90}" y="${Y(mz)+6}" font-size="9" fill="${pcb}" font-weight="700">PCB áp trong</text>`;
  s+=`<circle cx="${X(my)}" cy="${Y(mz)}" r="3.6" fill="${sensor}"/>`;

  // nón
  const ol=70;
  s+=`<polygon points="${X(my)},${Y(mz)} ${X(my+ol)},${Y(mz+ol*Math.tan(rad(RAY_LOW)))} ${X(my+ol)},${Y(mz+ol*Math.tan(rad(RAY_HIGH)))}"
       fill="${beam}" fill-opacity="0.16"/>`;
  s+=`<line x1="${X(my)}" y1="${Y(mz)}" x2="${X(my+ol)}" y2="${Y(mz)}" stroke="${good}" stroke-width="2.6"/>`;
  s+=`<text x="${X(my+ol)+5}" y="${Y(mz)+4}" font-size="9.5" fill="${good}" font-weight="700">tia thấp 0° ∥ băng</text>`;
  s+=`<line x1="${X(my)}" y1="${Y(mz)}" x2="${X(my+ol)}" y2="${Y(mz+ol*Math.tan(rad(AXIS)))}" stroke="${beam}" stroke-width="1.6" stroke-dasharray="5 4"/>`;
  s+=`<text x="${X(my+ol)+5}" y="${Y(mz+ol*Math.tan(rad(AXIS)))}" font-size="9" fill="${beam}">trục ${AXIS}°</text>`;
  s+=`<line x1="${X(my)}" y1="${Y(mz)}" x2="${X(my+ol)}" y2="${Y(mz+ol*Math.tan(rad(RAY_HIGH)))}" stroke="${beam}" stroke-width="1.2" stroke-dasharray="2 3"/>`;
  s+=`<text x="${X(my+ol)+5}" y="${Y(mz+ol*Math.tan(rad(RAY_HIGH)))}" font-size="9" fill="${beam}">${RAY_HIGH}°</text>`;

  // cung góc mặt lắp so ngang
  s+=`<line x1="${X(my)}" y1="${Y(0)}" x2="${X(my)}" y2="${Y(mz)}" stroke="${muted}" stroke-dasharray="2 2"/>`;
  s+=`<text x="${X(my)+5}" y="${Y(mz/2)}" font-size="9" fill="${muted}">z=${H_SENS}mm</text>`;
  s+=`<text x="${X(P2[0])-34}" y="${Y(P2[1]+zOff)-6}" font-size="10" fill="${good}" font-weight="700">mặt lắp 77.5° so ngang</text>`;
  s+=`<text x="${X(P2[0])-34}" y="${Y(P2[1]+zOff)+6}" font-size="8.5" fill="${muted}">(ngả ${SEAT_LEAN}° khỏi phương đứng)</text>`;
  s+=`<text x="20" y="22" font-size="11.5" fill="${txt}" font-weight="700">Mặt cắt qua trạm sensor</text>`;
  s+=`</svg>`;
  return s;
}

document.getElementById('figAngles').innerHTML = drawAngles();
document.getElementById('figSeg').innerHTML = drawSegment();
document.getElementById('figStation').innerHTML = drawStation();
document.getElementById('figSection').innerHTML = drawSection();

document.getElementById('angleNotes').innerHTML =
  `<li>Mặt lắp hợp <b>77.5° với mặt ngang</b> — tức <b>gần thẳng đứng</b>, chỉ ngả ra sau ${SEAT_LEAN}° khỏi phương đứng.</li>`
  + `<li>Pháp tuyến (trục quang) ngẩng <b>${AXIS}°</b> ⇒ tia thấp nhất <b>${RAY_LOW}°</b> = song song mặt băng. ✓</li>`
  + `<li>Tia thấp nhất giữ <b>z=${H_SENS}mm</b> suốt bề rộng — không dâng, không hạ. Vật cao hơn ${H_SENS}mm bị bắt ở mọi vị trí ngang.</li>`;

document.getElementById('segNotes').innerHTML =
  `<li>Rail 800mm chia <b>4 đoạn × ${SEG_L}mm</b>, mỗi đoạn ${SENS_PER_SEG} sensor pitch ${PITCH}mm.</li>`
  + `<li>Thân nêm: rộng <b>${BODY_W}mm</b>, cao <b>${BODY_H.toFixed(1)}mm</b>, mặt lắp dài <b>${SEAT_H}mm</b> ngả ${SEAT_LEAN}° khỏi đứng.</li>`
  + `<li>Ghép bằng mộng chồng + 2 chốt Ø3mm. <b>Chốt lo định vị góc</b> — mối nối lệch sẽ làm cả đoạn sau lệch hướng tia.</li>`;

document.getElementById('stationNotes').innerHTML =
  `<li>Hốc chìm <b>${PCB_L} × ${PCB_W} × ${PCB_T}mm</b> (+0.3mm dung sai FDM). Định vị bằng hình học, vít chỉ giữ.</li>`
  + `<li>2 lỗ M2 pitch <b>${HOLE_PITCH}mm</b> khớp 2 tai tròn của board tím.</li>`
  + `<li><b>PCB lắp từ mặt trong</b>, chip nhìn qua cửa sổ ${WIN_W}×${WIN_H}mm → không vật liệu nào trong đường quang.</li>`
  + `<li>Header 6 chân quay vào khoang. GPIO1 không dùng trong code — để trống.</li>`;

const rows = [
  ['200mm (gần ray)', slant(200)],
  ['325mm (giữa băng)', slant(325)],
  ['650mm (mép băng xa)', slant(650)],
  ['780mm (target strip)', slant(780)],
];
document.getElementById('covTable').innerHTML =
  `<tr><th>Khoảng cách ngang</th><th>Slant range (÷cos${AXIS}°)</th><th>So ngưỡng ${THRESH}mm</th></tr>`
  + rows.map(r=>{
      const ok=r[1]<THRESH, col=ok?css('--good'):css('--critical');
      return `<tr><td>${r[0]}</td><td><b>${r[1].toFixed(0)}mm</b></td><td style="color:${col};font-weight:600">${ok?'detected ✓':'ngoài ngưỡng ✓ (đúng ý đồ)'}</td></tr>`;
    }).join('');

document.getElementById('printNotes').innerHTML =
  `<li><b>In 1 đoạn trước.</b> Đủ để kiểm PCB có vừa hốc, vít có ăn, và góc tia thực tế.</li>`
  + `<li>Test góc quan trọng nhất: đặt mount trên mặt phẳng, để một tấm bìa <b>nằm ngang cao ${H_SENS}mm</b> chạy ra xa. Nếu góc đúng, sensor <b>không</b> đọc thấy tấm bìa (tia song song, chỉ lướt trên). Đọc thấy ⇒ tia đang chúc xuống ⇒ góc in sai.</li>`
  + `<li>Test crosstalk: đo cùng target khi có mount và khi cầm tay trần. Lệch đáng kể ⇒ khoét cửa sổ rộng thêm.</li>`
  + `<li>In <b>đặt nằm</b> (mặt đáy xuống bàn), layer 0.2mm, 3 perimeter, infill 20%. Mặt lắp gần đứng — in thẳng đứng thì mặt nhẵn, không cần support.</li>`
  + `<li>Vật liệu <b>PETG đen nhám</b>: dai hơn PLA khi trẻ va, chịu ấm gần motor, ít phản xạ.</li>`;
