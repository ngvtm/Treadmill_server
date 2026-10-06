// Gờ lăng trụ tam giác rỗng, góc nhọn 20 độ, dài 800mm, mount 8 sensor VL53L0X
// dọc hông băng chạy 800 x 650 mm. Mọi số tính từ hằng số dưới đây.
function css(v){ return getComputedStyle(document.documentElement).getPropertyValue(v).trim(); }

// ---- tham số ----
const BELT_L = 800, BELT_W = 650;   // mm
const TILT   = 20;                  // độ, góc nhọn lăng trụ = góc nghiêng mặt huyền
const HALF   = 12.5;                // độ, nửa FoV VL53L0X
const N      = 8;                   // số sensor
const PRISM_L = BELT_L;             // dài lăng trụ
const BASE_W  = 60;                 // mm, bề rộng đáy tam giác
const H_APEX  = BASE_W*Math.tan(TILT*Math.PI/180);  // chiều cao cạnh đứng
const H_SENS  = 22;                 // mm, tâm quang sensor so mặt băng
const THRESH  = 700;                // mm, VL53_DETECT_MAX_MM mới
const TARGET_D = 780;               // mm, vị trí target strip (ngang)

const rad = d => d*Math.PI/180;
const zLow  = d => H_SENS + d*Math.tan(rad(TILT-HALF));
const zMid  = d => H_SENS + d*Math.tan(rad(TILT));
const zHigh = d => H_SENS + d*Math.tan(rad(TILT+HALF));
const slant = d => d/Math.cos(rad(TILT));

const PITCH = BELT_L/N;
const sxs = [];
for(let i=0;i<N;i++) sxs.push(PITCH/2 + i*PITCH);
const ROLE  = ['SAFE','SAFE','WARN1','WARN1','WARN2','WARN2','DANGER','DANGER'];
const RCOL  = { SAFE:'--good', WARN1:'--warning', WARN2:'--warning', DANGER:'--critical' };

// ---------- A. isometric ----------
function drawIso(){
  const belt=css('--belt'),axis=css('--axis'),beam=css('--beam'),sensor=css('--sensor'),
        muted=css('--muted'),child=css('--child'),txt=css('--text-secondary'),
        fTop=css('--face-top'),fSide=css('--face-side'),fEnd=css('--face-end'),slot=css('--slot');
  const S=0.60;
  const PX=(x,y,z)=> 120 + x*S*0.87 + y*S*0.46;
  const PY=(x,y,z)=> 400 - x*S*0.26 + y*S*0.30 - z*S*0.92;
  const w=880,h=520;
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="lăng trụ 3D">`;

  // mặt băng
  const beltPts=[[0,0],[BELT_L,0],[BELT_L,BELT_W],[0,BELT_W]]
    .map(p=>`${PX(p[0],p[1],0)},${PY(p[0],p[1],0)}`).join(' ');
  s+=`<polygon points="${beltPts}" fill="${belt}" stroke="${axis}" stroke-width="1.2"/>`;
  for(let x=100;x<BELT_L;x+=100){
    s+=`<line x1="${PX(x,0,0)}" y1="${PY(x,0,0)}" x2="${PX(x,BELT_W,0)}" y2="${PY(x,BELT_W,0)}"
         stroke="${axis}" stroke-opacity="0.3"/>`;
  }

  // mặt phẳng quét (vẽ trước để lăng trụ đè lên)
  sxs.forEach((x,i)=>{
    const quad=[[x,0,H_SENS],[x,BELT_W,zLow(BELT_W)],[x,BELT_W,zHigh(BELT_W)]]
      .map(p=>`${PX(p[0],p[1],p[2])},${PY(p[0],p[1],p[2])}`).join(' ');
    s+=`<polygon points="${quad}" fill="${beam}" fill-opacity="0.17" stroke="${beam}" stroke-opacity="0.4"/>`;
    s+=`<line x1="${PX(x,0,H_SENS)}" y1="${PY(x,0,H_SENS)}"
         x2="${PX(x,BELT_W,zMid(BELT_W))}" y2="${PY(x,BELT_W,zMid(BELT_W))}"
         stroke="${beam}" stroke-opacity="0.7" stroke-dasharray="5 4"/>`;
  });

  // ---- LĂNG TRỤ: tam giác vuông, đáy y=[-BASE_W,0], apex nhọn 20deg ở y=0 ----
  // đỉnh: A=(-BASE_W,0) đáy ngoài, B=(0,0) mũi nhọn phía băng, C=(-BASE_W,H_APEX) đỉnh cạnh đứng
  // mặt huyền nối B(y=0,z=0) -> C(y=-BASE_W,z=H_APEX)
  const yA=-BASE_W, yB=0, zC=H_APEX;
  // mặt đáy
  const bot=[[0,yA,0],[PRISM_L,yA,0],[PRISM_L,yB,0],[0,yB,0]]
    .map(p=>`${PX(p[0],p[1],p[2])},${PY(p[0],p[1],p[2])}`).join(' ');
  s+=`<polygon points="${bot}" fill="${fEnd}" stroke="${axis}" stroke-width="0.8"/>`;
  // mặt đứng ngoài (y=yA)
  const backF=[[0,yA,0],[PRISM_L,yA,0],[PRISM_L,yA,zC],[0,yA,zC]]
    .map(p=>`${PX(p[0],p[1],p[2])},${PY(p[0],p[1],p[2])}`).join(' ');
  s+=`<polygon points="${backF}" fill="${fSide}" stroke="${axis}" stroke-width="0.8"/>`;
  // MẶT HUYỀN (chứa rãnh) nối (y=yB,z=0) -> (y=yA,z=zC)
  const hyp=[[0,yB,0],[PRISM_L,yB,0],[PRISM_L,yA,zC],[0,yA,zC]]
    .map(p=>`${PX(p[0],p[1],p[2])},${PY(p[0],p[1],p[2])}`).join(' ');
  s+=`<polygon points="${hyp}" fill="${fTop}" stroke="${axis}" stroke-width="1"/>`;
  // mặt đầu bịt (x=0) - tam giác, cho thấy ruột rỗng
  const endTri=[[0,yA,0],[0,yB,0],[0,yA,zC]]
    .map(p=>`${PX(p[0],p[1],p[2])},${PY(p[0],p[1],p[2])}`).join(' ');
  s+=`<polygon points="${endTri}" fill="${fEnd}" stroke="${axis}" stroke-width="1"/>`;
  // ruột rỗng (tam giác nhỏ hơn, offset vào 4mm)
  const t=5;
  const inTri=[[0,yA+t*2.2,t*0.5],[0,yB-t*2.6,t*0.5],[0,yA+t*2.2,zC-t*1.4]]
    .map(p=>`${PX(p[0],p[1],p[2])},${PY(p[0],p[1],p[2])}`).join(' ');
  s+=`<polygon points="${inTri}" fill="${slot}" fill-opacity="0.55" stroke="${axis}" stroke-opacity="0.5"/>`;
  s+=`<text x="${PX(0,yA+26,zC*0.42)-30}" y="${PY(0,yA+26,zC*0.42)+4}" font-size="9" fill="${muted}">rỗng: đi dây</text>`;

  // rãnh + sensor trên mặt huyền: đặt tại trung điểm mặt huyền
  const ym=(yA+yB)/2, zm=zC/2;
  sxs.forEach((x,i)=>{
    const rc=css(RCOL[ROLE[i]]);
    // rãnh chữ nhật nhỏ trên mặt huyền
    const sw=26, sh=9;
    const rect=[[x-sw/2,ym+sh,zm+sh*0.36],[x+sw/2,ym+sh,zm+sh*0.36],
                [x+sw/2,ym-sh,zm-sh*0.36],[x-sw/2,ym-sh,zm-sh*0.36]]
      .map(p=>`${PX(p[0],p[1],p[2])},${PY(p[0],p[1],p[2])}`).join(' ');
    s+=`<polygon points="${rect}" fill="${slot}" stroke="${rc}" stroke-width="1.3"/>`;
    // sensor điểm sáng ở tâm rãnh
    s+=`<circle cx="${PX(x,ym,zm)}" cy="${PY(x,ym,zm)}" r="3.2" fill="${sensor}"/>`;
    // nhãn
    s+=`<text x="${PX(x,ym,zm)-3}" y="${PY(x,ym,zm)-13}" font-size="9" fill="${txt}" font-weight="700">S${i}</text>`;
    s+=`<text x="${PX(x,ym,zm)-13}" y="${PY(x,yA,zC)-20}" font-size="7" fill="${rc}" font-weight="700">${ROLE[i]}</text>`;
  });

  // target strip phía đối diện
  const zt=zMid(TARGET_D);
  const tgt=[[0,TARGET_D,zLow(TARGET_D)],[PRISM_L,TARGET_D,zLow(TARGET_D)],
             [PRISM_L,TARGET_D,zHigh(TARGET_D)],[0,TARGET_D,zHigh(TARGET_D)]]
    .map(p=>`${PX(p[0],p[1],p[2])},${PY(p[0],p[1],p[2])}`).join(' ');
  s+=`<polygon points="${tgt}" fill="${css('--good')}" fill-opacity="0.30" stroke="${css('--good')}" stroke-width="1.2"/>`;
  s+=`<text x="${PX(PRISM_L*0.5,TARGET_D,zHigh(TARGET_D))+8}" y="${PY(PRISM_L*0.5,TARGET_D,zHigh(TARGET_D))-6}"
       font-size="10" fill="${css('--good')}" font-weight="700">target strip @ ${TARGET_D}mm</text>`;

  // trẻ
  const cxp=sxs[2], cyp=BELT_W/2;
  s+=`<ellipse cx="${PX(cxp,cyp,0)}" cy="${PY(cxp,cyp,0)}" rx="20" ry="9" fill="${child}" fill-opacity="0.22"/>`;
  s+=`<line x1="${PX(cxp,cyp,0)}" y1="${PY(cxp,cyp,0)}" x2="${PX(cxp,cyp,540)}" y2="${PY(cxp,cyp,540)}"
       stroke="${child}" stroke-width="13" stroke-opacity="0.5" stroke-linecap="round"/>`;
  s+=`<circle cx="${PX(cxp,cyp,600)}" cy="${PY(cxp,cyp,600)}" r="8" fill="${child}" fill-opacity="0.6"/>`;

  // nhãn kích thước
  s+=`<text x="${PX(BELT_L*0.5,BELT_W,0)+10}" y="${PY(BELT_L*0.5,BELT_W,0)+26}" font-size="11" fill="${muted}">rộng 650mm</text>`;
  s+=`<text x="${PX(BELT_L,BELT_W*0.4,0)+14}" y="${PY(BELT_L,BELT_W*0.4,0)-2}" font-size="11" fill="${muted}">dài 800mm →</text>`;
  s+=`<text x="22" y="26" font-size="12.5" fill="${txt}" font-weight="700">Lăng trụ tam giác rỗng 20°, dài 800mm — 8 rãnh sensor trên mặt huyền</text>`;
  s+=`<text x="22" y="44" font-size="10.5" fill="${muted}">Mỗi sensor phát một mặt phẳng quét cắt ngang băng. 8 lát cắt độc lập dọc chiều dài.</text>`;
  s+=`</svg>`;
  return s;
}

// ---------- B. profile ----------
function drawProfile(){
  const axis=css('--axis'),muted=css('--muted'),txt=css('--text-secondary'),
        fTop=css('--face-top'),fSide=css('--face-side'),slot=css('--slot'),
        sensor=css('--sensor'),beam=css('--beam'),good=css('--good'),belt=css('--belt');
  const P=3.0, M=70;
  const w=560, h=240;
  const X=mm=>M+mm*P, Y=mm=>h-M-mm*P;
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="mặt cắt lăng trụ">`;

  // mặt băng
  s+=`<rect x="${X(-BASE_W-14)}" y="${Y(0)}" width="${(BASE_W+150)*P}" height="7" fill="${belt}" stroke="${axis}"/>`;

  // tam giác: A(-BASE_W,0) B(0,0) C(-BASE_W,H_APEX)
  s+=`<polygon points="${X(-BASE_W)},${Y(0)} ${X(0)},${Y(0)} ${X(-BASE_W)},${Y(H_APEX)}"
       fill="${fTop}" stroke="${axis}" stroke-width="1.5"/>`;
  // ruột rỗng
  s+=`<polygon points="${X(-BASE_W+9)},${Y(4)} ${X(-11)},${Y(4)} ${X(-BASE_W+9)},${Y(H_APEX-7)}"
       fill="${slot}" fill-opacity="0.5" stroke="${axis}" stroke-opacity="0.5" stroke-dasharray="3 2"/>`;
  s+=`<text x="${X(-BASE_W+15)}" y="${Y(9)}" font-size="8" fill="${muted}">rỗng</text>`;

  // sensor tại trung điểm mặt huyền
  const ym=-BASE_W/2, zmm=H_APEX/2;
  s+=`<rect x="${X(ym)-9}" y="${Y(zmm)-5}" width="18" height="10" fill="${slot}" stroke="${sensor}" stroke-width="1.3"
       transform="rotate(${-TILT} ${X(ym)} ${Y(zmm)})"/>`;
  s+=`<circle cx="${X(ym)}" cy="${Y(zmm)}" r="3.2" fill="${sensor}"/>`;

  // tia
  const dd=150;
  s+=`<polygon points="${X(ym)},${Y(zmm)} ${X(ym+dd)},${Y(zmm+dd*Math.tan(rad(TILT-HALF)))} ${X(ym+dd)},${Y(zmm+dd*Math.tan(rad(TILT+HALF)))}"
       fill="${beam}" fill-opacity="0.2" stroke="${beam}" stroke-opacity="0.45"/>`;
  s+=`<line x1="${X(ym)}" y1="${Y(zmm)}" x2="${X(ym+dd)}" y2="${Y(zmm+dd*Math.tan(rad(TILT)))}"
       stroke="${beam}" stroke-dasharray="4 3"/>`;

  // cung góc 20 độ ở mũi nhọn B
  s+=`<path d="M ${X(-26)} ${Y(0)} A 26 26 0 0 1 ${X(-26*Math.cos(rad(TILT)))} ${Y(26*Math.sin(rad(TILT)))}"
       fill="none" stroke="${good}" stroke-width="1.6"/>`;
  s+=`<text x="${X(-34)}" y="${Y(6)}" font-size="11" fill="${good}" font-weight="700">20°</text>`;

  // kích thước
  s+=`<line x1="${X(-BASE_W)}" y1="${Y(-9)}" x2="${X(0)}" y2="${Y(-9)}" stroke="${muted}" stroke-width="1"/>`;
  s+=`<text x="${X(-BASE_W/2)}" y="${Y(-15)}" text-anchor="middle" font-size="9.5" fill="${muted}">đáy ${BASE_W}mm</text>`;
  s+=`<line x1="${X(-BASE_W-9)}" y1="${Y(0)}" x2="${X(-BASE_W-9)}" y2="${Y(H_APEX)}" stroke="${muted}"/>`;
  s+=`<text x="${X(-BASE_W-13)}" y="${Y(H_APEX/2)}" text-anchor="end" font-size="9.5" fill="${muted}">${H_APEX.toFixed(1)}mm</text>`;
  s+=`<text x="${X(ym)+16}" y="${Y(zmm)+22}" font-size="9" fill="${txt}">tâm quang z=${H_SENS}mm</text>`;

  s+=`<text x="${X(60)}" y="${Y(H_APEX*1.6)}" font-size="10" fill="${muted}">mặt huyền = ${(BASE_W/Math.cos(rad(TILT))).toFixed(1)}mm</text>`;
  s+=`<text x="20" y="20" font-size="11.5" fill="${txt}" font-weight="700">Biên dạng extrude 800mm</text>`;
  s+=`</svg>`;
  return s;
}

// ---------- C. rãnh trên mặt huyền ----------
function drawSlots(){
  const axis=css('--axis'),muted=css('--muted'),txt=css('--text-secondary'),
        fTop=css('--face-top'),slot=css('--slot'),sensor=css('--sensor');
  const HYP = BASE_W/Math.cos(rad(TILT));
  const P=0.86, M=52;
  const w=BELT_L*P+M*2, h=HYP*P+M*2+34;
  const X=mm=>M+mm*P, Y=mm=>M+20+mm*P;
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="rãnh sensor">`;

  s+=`<rect x="${X(0)}" y="${Y(0)}" width="${BELT_L*P}" height="${HYP*P}" fill="${fTop}" stroke="${axis}" stroke-width="1.3" rx="3"/>`;

  const SW=13, SH=22;  // rãnh cửa sổ quang
  sxs.forEach((x,i)=>{
    const rc=css(RCOL[ROLE[i]]);
    const cy=HYP/2;
    // hốc chìm module (25.4 x 10.8 mm cho VL53L0X GY-530)
    s+=`<rect x="${X(x-13)}" y="${Y(cy-5.4)}" width="${26*P}" height="${10.8*P}"
         fill="none" stroke="${axis}" stroke-dasharray="3 2" stroke-opacity="0.75"/>`;
    // cửa sổ quang
    s+=`<rect x="${X(x-SW/2)}" y="${Y(cy-SH/2)}" width="${SW*P}" height="${SH*P}" fill="${slot}" stroke="${rc}" stroke-width="1.4" rx="1.5"/>`;
    // 2 lỗ vít M2
    s+=`<circle cx="${X(x-16)}" cy="${Y(cy)}" r="${1.1*P*2}" fill="none" stroke="${axis}"/>`;
    s+=`<circle cx="${X(x+16)}" cy="${Y(cy)}" r="${1.1*P*2}" fill="none" stroke="${axis}"/>`;
    s+=`<text x="${X(x)}" y="${Y(-7)}" text-anchor="middle" font-size="9.5" fill="${txt}" font-weight="700">S${i}</text>`;
    s+=`<text x="${X(x)}" y="${Y(HYP+13)}" text-anchor="middle" font-size="7.5" fill="${rc}" font-weight="700">${ROLE[i]}</text>`;
  });

  // pitch
  s+=`<line x1="${X(sxs[0])}" y1="${Y(HYP+22)}" x2="${X(sxs[1])}" y2="${Y(HYP+22)}" stroke="${muted}"/>`;
  s+=`<text x="${X((sxs[0]+sxs[1])/2)}" y="${Y(HYP+31)}" text-anchor="middle" font-size="9" fill="${muted}">pitch ${PITCH}mm</text>`;
  s+=`<text x="${X(BELT_L/2)}" y="18" text-anchor="middle" font-size="11" fill="${muted}">mặt huyền: ${BELT_L} × ${HYP.toFixed(1)}mm</text>`;
  s+=`</svg>`;
  return s;
}

// ---------- D. vùng phủ ----------
function drawCov(){
  const belt=css('--belt'),axis=css('--axis'),beam=css('--beam'),muted=css('--muted'),
        sensor=css('--sensor'),good=css('--good'),bad=css('--critical'),txt=css('--text-secondary'),
        child=css('--child'),warn=css('--warning');
  const P=0.60, M=58, HT=620;
  const w=(TARGET_D+90)*P+M*2, h=HT*P+M*2;
  const X=mm=>M+mm*P, Y=mm=>(M+HT*P)-mm*P;
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img">`;

  for(let z=0;z<=600;z+=100){
    s+=`<line x1="${X(0)}" y1="${Y(z)}" x2="${X(TARGET_D+70)}" y2="${Y(z)}" stroke="${css('--grid')}"/>`;
    s+=`<text x="${X(0)-8}" y="${Y(z)+3.5}" text-anchor="end" font-size="8.5" fill="${muted}">${z}</text>`;
  }
  s+=`<rect x="${X(0)}" y="${Y(0)}" width="${BELT_W*P}" height="7" fill="${belt}" stroke="${axis}"/>`;

  const zl=zLow(BELT_W), zh=zHigh(BELT_W);
  s+=`<polygon points="${X(0)},${Y(H_SENS)} ${X(TARGET_D)},${Y(zLow(TARGET_D))} ${X(TARGET_D)},${Y(zHigh(TARGET_D))}"
       fill="${beam}" fill-opacity="0.17" stroke="${beam}" stroke-opacity="0.4"/>`;
  s+=`<line x1="${X(0)}" y1="${Y(H_SENS)}" x2="${X(TARGET_D)}" y2="${Y(zMid(TARGET_D))}" stroke="${beam}" stroke-dasharray="5 4" stroke-opacity="0.8"/>`;
  s+=`<circle cx="${X(0)}" cy="${Y(H_SENS)}" r="5" fill="${sensor}"/>`;

  // mép băng
  s+=`<line x1="${X(BELT_W)}" y1="${Y(0)}" x2="${X(BELT_W)}" y2="${Y(560)}" stroke="${axis}" stroke-width="1.5" stroke-dasharray="4 3"/>`;
  s+=`<text x="${X(BELT_W)-5}" y="${Y(575)}" text-anchor="end" font-size="9.5" fill="${muted}">mép băng 650mm</text>`;
  s+=`<text x="${X(BELT_W)-5}" y="${Y(548)}" text-anchor="end" font-size="9" fill="${good}" font-weight="700">slant ${slant(BELT_W).toFixed(0)}mm &lt; ${THRESH} ✓</text>`;

  // target strip
  s+=`<rect x="${X(TARGET_D)}" y="${Y(zHigh(TARGET_D))}" width="9" height="${(zHigh(TARGET_D)-zLow(TARGET_D))*P}"
       fill="${good}" fill-opacity="0.5" stroke="${good}"/>`;
  s+=`<text x="${X(TARGET_D)+14}" y="${Y(zMid(TARGET_D))-6}" font-size="9.5" fill="${good}" font-weight="700">target strip</text>`;
  s+=`<text x="${X(TARGET_D)+14}" y="${Y(zMid(TARGET_D))+6}" font-size="9" fill="${muted}">baseline ${slant(TARGET_D).toFixed(0)}mm</text>`;

  // ngưỡng phát hiện, quy về khoảng cách ngang
  const dTh = THRESH*Math.cos(rad(TILT));
  s+=`<line x1="${X(dTh)}" y1="${Y(0)}" x2="${X(dTh)}" y2="${Y(600)}" stroke="${bad}" stroke-width="2" stroke-dasharray="6 4"/>`;
  s+=`<text x="${X(dTh)+6}" y="${Y(592)}" font-size="9.5" fill="${bad}" font-weight="700">ngưỡng ${THRESH}mm</text>`;
  s+=`<text x="${X(dTh)+6}" y="${Y(568)}" font-size="9" fill="${bad}">= ngang ${dTh.toFixed(0)}mm</text>`;

  // trẻ đứng giữa băng
  const cw=120, cx0=BELT_W/2-cw/2;
  s+=`<rect x="${X(cx0)}" y="${Y(600)}" width="${cw*P}" height="${600*P}" rx="7" fill="${child}" fill-opacity="0.32"/>`;
  s+=`<circle cx="${X(cx0)}" cy="${Y(zMid(cx0))}" r="4.5" fill="${warn}" stroke="#fff" stroke-width="1.3"/>`;
  s+=`<text x="${X(cx0)+10}" y="${Y(zMid(cx0))-7}" font-size="9.5" fill="${warn}" font-weight="700">trẻ: slant ${slant(cx0).toFixed(0)}mm</text>`;

  s+=`<text x="${X((TARGET_D)/2)}" y="${h-12}" text-anchor="middle" font-size="10.5" fill="${muted}">trục ngang: khoảng cách ngang từ sensor (mm) — trục đứng: chiều cao (mm)</text>`;
  s+=`<text x="20" y="20" font-size="11.5" fill="${txt}" font-weight="700">Vùng phủ &amp; vị trí ngưỡng ${THRESH}mm</text>`;
  s+=`</svg>`;
  return s;
}

document.getElementById('figIso').innerHTML = drawIso();
document.getElementById('figProfile').innerHTML = drawProfile();
document.getElementById('figSlots').innerHTML = drawSlots();
document.getElementById('figCov').innerHTML = drawCov();

const HYP = BASE_W/Math.cos(rad(TILT));
document.getElementById('isoNotes').innerHTML =
  `<li>Lăng trụ dài <b>${PRISM_L}mm</b> = đúng chiều dài băng, dán/bắt vít dọc hông.</li>`
  + `<li>8 sensor pitch <b>${PITCH}mm</b>, vai trò 2-2-2-2: SAFE → WARN1 → WARN2 → DANGER theo chiều băng.</li>`
  + `<li>Ruột rỗng chạy suốt để đi bus I2C (SDA/SCL) + 8 dây XSHUT + nguồn — dây không lộ ra vùng trẻ chạy.</li>`;

document.getElementById('profNotes').innerHTML =
  `<li>Tam giác vuông: đáy <b>${BASE_W}mm</b>, cạnh đứng <b>${H_APEX.toFixed(1)}mm</b>, mặt huyền <b>${HYP.toFixed(1)}mm</b>, góc nhọn <b>20°</b>.</li>`
  + `<li>Mũi nhọn 20° quay về phía băng → không có cạnh sắc chĩa vào trẻ, và mặt huyền tự động cho sensor đúng góc ngẩng 20°.</li>`
  + `<li>Tâm quang sensor ở <b>z=${H_SENS}mm</b>. Tia thấp nhất (20−12.5=7.5°) tại mép xa: <b>z=${zLow(BELT_W).toFixed(0)}mm</b> — cao hơn mặt băng nên không liếm mặt băng.</li>`;

document.getElementById('slotNotes').innerHTML =
  `<li>Hốc chìm <b>26 × 10.8mm</b> (nét đứt) ôm thân module GY-530; cửa sổ quang <b>13 × 22mm</b> thông ra ngoài.</li>`
  + `<li>2 lỗ vít M2 cách nhau 32mm mỗi rãnh. Định vị bằng hốc chìm chứ không chỉ dựa vít.</li>`
  + `<li>Thành cửa sổ nên <b>nhám, đen tuyệt đối</b> để tránh optical crosstalk (ánh sáng emitter dội vào receiver làm sai khoảng cách).</li>`
  + `<li>Cửa sổ càng sâu thì FoV càng hẹp — có thể dùng để giảm chồng lấn giữa các lát cắt nếu cần.</li>`;

const rows = [
  ['200mm (gần ray)', slant(200)],
  ['325mm (giữa băng)', slant(325)],
  ['500mm', slant(500)],
  ['650mm (mép băng xa)', slant(650)],
  ['780mm (target strip)', slant(780)],
];
document.getElementById('covTable').innerHTML =
  `<tr><th>Khoảng cách ngang</th><th>Slant range sensor đọc</th><th>So với ngưỡng ${THRESH}mm</th></tr>`
  + rows.map(r=>{
      const ok = r[1] < THRESH;
      const col = ok ? css('--good') : css('--critical');
      const lbl = ok ? 'detected ✓' : 'ngoài ngưỡng — không detect';
      return `<tr><td>${r[0]}</td><td><b>${r[1].toFixed(0)}mm</b></td><td style="color:${col};font-weight:600">${lbl}</td></tr>`;
    }).join('');
