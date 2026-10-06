// Sensor lắp dọc hông băng, quét ngang. Hình học thật, mọi số đều tính ra chứ không hardcode.
// Băng 98 (dài) x 63 (rộng) cm. Sensor cao h=6cm, tilt 25 độ, FoV 25 độ (+-12.5).
function css(v){ return getComputedStyle(document.documentElement).getPropertyValue(v).trim(); }

const L = 98, WID = 63;          // cm
const H_SENS = 6;                // cm, tâm sensor so mặt băng
const TILT = 25;                 // độ, ngẩng lên so phương ngang
const HALF = 12.5;               // độ, nửa FoV
const THRESH = 90;               // cm, VL53_DETECT_MAX_MM = 900mm
const N = 6;
const rad = d => d*Math.PI/180;

// Vị trí 6 sensor dọc chiều dài (pitch đều)
const PITCH = L/N;
const sx = [];
for(let i=0;i<N;i++) sx.push(PITCH/2 + i*PITCH);
const ROLE = ['SAFE','SAFE','WARN1','WARN2','DANGER','DANGER'];

// Độ cao tia tại khoảng cách ngang d (cm)
const zLow  = d => H_SENS + d*Math.tan(rad(TILT-HALF));   // tia thấp nhất, 12.5 độ
const zMid  = d => H_SENS + d*Math.tan(rad(TILT));        // trục quang, 25 độ
const zHigh = d => H_SENS + d*Math.tan(rad(TILT+HALF));   // tia cao nhất, 37.5 độ
// Khoảng cách nghiêng (slant range) tới điểm cách ngang d trên trục quang
const slant = d => d/Math.cos(rad(TILT));
// Khoảng cách ngang tương ứng với slant = THRESH
const dAtThresh = THRESH*Math.cos(rad(TILT));

// ---------- A. 3D isometric ----------
function draw3D(){
  const belt=css('--belt'),axis=css('--axis'),frame=css('--frame'),beam=css('--beam'),
        sensor=css('--sensor'),muted=css('--muted'),child=css('--child'),txt=css('--text-secondary');
  // phép chiếu iso: x dọc băng, y ngang băng, z cao
  const S=4.2, ax=0.86, ay=0.50;
  const PX=(x,y,z)=> 90 + x*S*ax + y*S*ay*0.86;
  const PY=(x,y,z)=> 300 - x*S*0.30 + y*S*0.34 - z*S*0.95;
  const w=760,h=430;
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="3D tổng thể">`;

  // mặt băng
  const c=[[0,0],[L,0],[L,WID],[0,WID]].map(p=>`${PX(p[0],p[1],0)},${PY(p[0],p[1],0)}`).join(' ');
  s+=`<polygon points="${c}" fill="${belt}" stroke="${axis}" stroke-width="1.2"/>`;
  // vân băng
  for(let x=10;x<L;x+=10){
    s+=`<line x1="${PX(x,0,0)}" y1="${PY(x,0,0)}" x2="${PX(x,WID,0)}" y2="${PY(x,WID,0)}" stroke="${axis}" stroke-opacity="0.35"/>`;
  }
  // gờ sensor dọc hông (y=0), cao H_SENS
  const bar=[[0,0,0],[L,0,0],[L,0,H_SENS+2],[0,0,H_SENS+2]].map(p=>`${PX(p[0],p[1],p[2])},${PY(p[0],p[1],p[2])}`).join(' ');
  s+=`<polygon points="${bar}" fill="${frame}" fill-opacity="0.85" stroke="${axis}"/>`;

  // mỗi sensor: mặt phẳng quét = tứ giác đứng cắt ngang băng
  sx.forEach((x,i)=>{
    const dEnd=WID;
    const quad=[
      [x,0,H_SENS],
      [x,dEnd,zLow(dEnd)],
      [x,dEnd,zHigh(dEnd)],
      [x,0,H_SENS]
    ].map(p=>`${PX(p[0],p[1],p[2])},${PY(p[0],p[1],p[2])}`).join(' ');
    s+=`<polygon points="${quad}" fill="${beam}" fill-opacity="0.20" stroke="${beam}" stroke-opacity="0.45"/>`;
    // trục quang
    s+=`<line x1="${PX(x,0,H_SENS)}" y1="${PY(x,0,H_SENS)}" x2="${PX(x,dEnd,zMid(dEnd))}" y2="${PY(x,dEnd,zMid(dEnd))}" stroke="${beam}" stroke-opacity="0.75" stroke-dasharray="4 3"/>`;
    s+=`<circle cx="${PX(x,0,H_SENS)}" cy="${PY(x,0,H_SENS)}" r="4" fill="${sensor}"/>`;
    s+=`<text x="${PX(x,0,H_SENS)-4}" y="${PY(x,0,H_SENS)-9}" font-size="9.5" fill="${txt}" font-weight="700">S${i}</text>`;
    s+=`<text x="${PX(x,0,H_SENS)-8}" y="${PY(x,0,H_SENS)+16}" font-size="7.5" fill="${muted}">${ROLE[i]}</text>`;
  });

  // trẻ đứng ở lát S1-S2
  const cxp=sx[2], cyp=WID/2;
  s+=`<ellipse cx="${PX(cxp,cyp,0)}" cy="${PY(cxp,cyp,0)}" rx="17" ry="8" fill="${child}" fill-opacity="0.25"/>`;
  s+=`<line x1="${PX(cxp,cyp,0)}" y1="${PY(cxp,cyp,0)}" x2="${PX(cxp,cyp,60)}" y2="${PY(cxp,cyp,60)}" stroke="${child}" stroke-width="11" stroke-opacity="0.55" stroke-linecap="round"/>`;
  s+=`<circle cx="${PX(cxp,cyp,66)}" cy="${PY(cxp,cyp,66)}" r="7" fill="${child}" fill-opacity="0.65"/>`;
  s+=`<text x="${PX(cxp,cyp,74)+10}" y="${PY(cxp,cyp,74)}" font-size="10" fill="${txt}">trẻ 50–70cm</text>`;

  // nhãn
  s+=`<text x="${PX(L/2,WID,0)+12}" y="${PY(L/2,WID,0)+22}" font-size="11" fill="${muted}">rộng 63cm</text>`;
  s+=`<text x="${PX(L,WID/2,0)+16}" y="${PY(L,WID/2,0)-4}" font-size="11" fill="${muted}">dài 98cm →</text>`;
  s+=`<text x="20" y="26" font-size="12" fill="${txt}" font-weight="700">Gờ sensor dọc hông băng — 6 mặt phẳng quét cắt ngang</text>`;
  s+=`<text x="20" y="44" font-size="10.5" fill="${muted}">Mỗi mặt phẳng = 1 lát cắt độc lập. Trẻ chắn lát nào → biết vị trí dọc băng.</text>`;
  s+=`</svg>`;
  return s;
}

// ---------- B. mặt cắt ngang ----------
function drawCross(){
  const belt=css('--belt'),axis=css('--axis'),frame=css('--frame'),beam=css('--beam'),
        sensor=css('--sensor'),muted=css('--muted'),child=css('--child'),
        bad=css('--critical'),good=css('--good'),txt=css('--text-secondary');
  const PXs=7.4, M=64, HT=62;
  const w=WID*PXs+M*2+150, h=HT*PXs+M*2;
  const X=cm=>M+cm*PXs, Y=cm=>(M+HT*PXs)-cm*PXs;
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="mặt cắt ngang">`;

  // lưới
  for(let z=0;z<=60;z+=10){
    s+=`<line x1="${X(0)}" y1="${Y(z)}" x2="${X(WID)}" y2="${Y(z)}" stroke="${css('--grid')}" stroke-width="1"/>`;
    s+=`<text x="${X(0)-8}" y="${Y(z)+3.5}" text-anchor="end" font-size="9" fill="${muted}">${z}</text>`;
  }
  // mặt băng
  s+=`<rect x="${X(0)}" y="${Y(0)}" width="${WID*PXs}" height="9" fill="${belt}" stroke="${axis}"/>`;
  // gờ sensor
  s+=`<rect x="${X(0)-16}" y="${Y(H_SENS+2)}" width="16" height="${(H_SENS+2)*PXs+9}" fill="${frame}"/>`;

  // nón quét
  const zl=zLow(WID), zh=zHigh(WID), zm=zMid(WID);
  s+=`<polygon points="${X(0)},${Y(H_SENS)} ${X(WID)},${Y(zl)} ${X(WID)},${Y(zh)}"
       fill="${beam}" fill-opacity="0.20" stroke="${beam}" stroke-opacity="0.5"/>`;
  s+=`<line x1="${X(0)}" y1="${Y(H_SENS)}" x2="${X(WID)}" y2="${Y(zm)}" stroke="${beam}" stroke-dasharray="5 3" stroke-opacity="0.85"/>`;
  s+=`<circle cx="${X(0)}" cy="${Y(H_SENS)}" r="5" fill="${sensor}"/>`;
  s+=`<text x="${X(0)-22}" y="${Y(H_SENS)-10}" font-size="10" fill="${txt}" font-weight="700">sensor</text>`;
  s+=`<text x="${X(0)-22}" y="${Y(H_SENS)+2}" font-size="9" fill="${muted}">h=${H_SENS}cm</text>`;

  // nhãn tia
  s+=`<text x="${X(WID)+6}" y="${Y(zh)+3}" font-size="9.5" fill="${beam}">tia cao 37.5° → z=${zh.toFixed(0)}cm</text>`;
  s+=`<text x="${X(WID)+6}" y="${Y(zm)+3}" font-size="9.5" fill="${beam}">trục 25° → z=${zm.toFixed(0)}cm</text>`;
  s+=`<text x="${X(WID)+6}" y="${Y(zl)+3}" font-size="9.5" fill="${beam}">tia thấp 12.5° → z=${zl.toFixed(0)}cm</text>`;

  // vùng mù dưới tia thấp
  s+=`<polygon points="${X(0)},${Y(H_SENS)} ${X(WID)},${Y(zl)} ${X(WID)},${Y(0)} ${X(0)},${Y(0)}"
       fill="${bad}" fill-opacity="0.10"/>`;
  s+=`<text x="${X(WID*0.55)}" y="${Y(4)}" font-size="9.5" fill="${bad}" font-weight="600">vùng mù (bàn chân rìa xa không thấy)</text>`;

  // trẻ đứng giữa
  const cw=13, cx0=WID/2-cw/2;
  s+=`<rect x="${X(cx0)}" y="${Y(62)}" width="${cw*PXs}" height="${62*PXs}" rx="6" fill="${child}" fill-opacity="0.42"/>`;
  s+=`<text x="${X(WID/2)}" y="${Y(40)}" text-anchor="middle" font-size="9.5" fill="#fff" font-weight="700">trẻ</text>`;
  // điểm sensor thực sự khóa vào: mép gần của trẻ
  const dHit=cx0;
  s+=`<circle cx="${X(dHit)}" cy="${Y(zMid(dHit))}" r="4.5" fill="${good}" stroke="#fff" stroke-width="1.4"/>`;
  s+=`<text x="${X(dHit)+8}" y="${Y(zMid(dHit))-8}" font-size="9.5" fill="${good}" font-weight="700">điểm đo: d=${dHit.toFixed(0)}cm, z=${zMid(dHit).toFixed(0)}cm</text>`;
  s+=`<text x="${X(dHit)+8}" y="${Y(zMid(dHit))+4}" font-size="9" fill="${muted}">slant ≈ ${slant(dHit).toFixed(0)}cm → detected</text>`;

  // đường ngưỡng 900mm (slant) chiếu về khoảng cách ngang
  s+=`<line x1="${X(dAtThresh)}" y1="${Y(0)}" x2="${X(dAtThresh)}" y2="${Y(60)}" stroke="${bad}" stroke-width="2" stroke-dasharray="6 4"/>`;
  s+=`<text x="${X(dAtThresh)+5}" y="${Y(57)}" font-size="9.5" fill="${bad}" font-weight="700">ngưỡng 900mm</text>`;
  s+=`<text x="${X(dAtThresh)+5}" y="${Y(53.5)}" font-size="9" fill="${bad}">= d ngang ${dAtThresh.toFixed(0)}cm</text>`;

  // mép băng đối diện
  s+=`<line x1="${X(WID)}" y1="${Y(0)}" x2="${X(WID)}" y2="${Y(60)}" stroke="${axis}" stroke-width="1.5"/>`;
  s+=`<text x="${X(WID)-4}" y="${Y(58)}" text-anchor="end" font-size="9.5" fill="${muted}">mép băng 63cm</text>`;

  s+=`<text x="${X(WID/2)}" y="${h-14}" text-anchor="middle" font-size="11" fill="${muted}">mặt cắt ngang băng — trục ngang: khoảng cách từ sensor (cm), trục đứng: chiều cao (cm)</text>`;
  s+=`</svg>`;
  return s;
}

// ---------- C. hai kịch bản ngưỡng ----------
function drawScenario(hasWall){
  const belt=css('--belt'),axis=css('--axis'),frame=css('--frame'),beam=css('--beam'),
        sensor=css('--sensor'),muted=css('--muted'),bad=css('--critical'),good=css('--good'),
        warn=css('--warning'),txt=css('--text-secondary');
  const PXs=4.6, M=44, HT=64, EXTRA=42;
  const w=(WID+EXTRA)*PXs+M*2, h=HT*PXs+M*2;
  const X=cm=>M+cm*PXs, Y=cm=>(M+HT*PXs)-cm*PXs;
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img">`;

  s+=`<rect x="${X(0)}" y="${Y(0)}" width="${WID*PXs}" height="7" fill="${belt}" stroke="${axis}"/>`;
  s+=`<rect x="${X(0)-11}" y="${Y(H_SENS+2)}" width="11" height="${(H_SENS+2)*PXs+7}" fill="${frame}"/>`;

  const zl=zLow(WID), zh=zHigh(WID), zm=zMid(WID);
  s+=`<polygon points="${X(0)},${Y(H_SENS)} ${X(WID)},${Y(zl)} ${X(WID)},${Y(zh)}"
       fill="${beam}" fill-opacity="0.16" stroke="${beam}" stroke-opacity="0.35"/>`;
  s+=`<circle cx="${X(0)}" cy="${Y(H_SENS)}" r="4" fill="${sensor}"/>`;

  // ngưỡng
  s+=`<line x1="${X(dAtThresh)}" y1="${Y(0)}" x2="${X(dAtThresh)}" y2="${Y(60)}" stroke="${bad}" stroke-width="1.6" stroke-dasharray="5 3"/>`;
  s+=`<text x="${X(dAtThresh)+4}" y="${Y(59)}" font-size="8.5" fill="${bad}" font-weight="700">900mm</text>`;

  let title, verdict, vcolor;
  if(hasWall){
    // tay vịn / khung cao chắn tia, nằm TRONG 900mm
    const dW=WID-2;
    s+=`<rect x="${X(dW)}" y="${Y(zh+4)}" width="9" height="${(zh+4)*PXs+7}" fill="${frame}" stroke="${axis}"/>`;
    s+=`<circle cx="${X(dW)}" cy="${Y(zm)}" r="4" fill="${bad}" stroke="#fff" stroke-width="1.2"/>`;
    s+=`<text x="${X(dW)+13}" y="${Y(zm)-4}" font-size="9" fill="${bad}" font-weight="700">tia chạm khung</text>`;
    s+=`<text x="${X(dW)+13}" y="${Y(zm)+6}" font-size="8.5" fill="${muted}">slant ${slant(dW).toFixed(0)}cm &lt; 90cm</text>`;
    title='C1. Phía đối diện CÓ khung/tay vịn cao';
    verdict=`is_detected() = TRUE mãi mãi → zone kẹt DANGER ngay cả khi băng trống`;
    vcolor=bad;
  } else {
    // không có gì, tia bay ra ngoài
    s+=`<line x1="${X(WID)}" y1="${Y(zm)}" x2="${X(WID+EXTRA-4)}" y2="${Y(zMid(WID+EXTRA-4))}" stroke="${beam}" stroke-dasharray="3 3" stroke-opacity="0.6"/>`;
    s+=`<text x="${X(WID+3)}" y="${Y(zMid(WID+EXTRA-4))-6}" font-size="9" fill="${warn}" font-weight="700">tia bay ra phòng</text>`;
    s+=`<text x="${X(WID+3)}" y="${Y(zMid(WID+EXTRA-4))+4}" font-size="8.5" fill="${muted}">không nền cố định</text>`;
    title='C2. Phía đối diện TRỐNG (khung thấp)';
    verdict=`đọc tường xa / status 4 → không phân biệt được "băng trống" với "sensor hỏng"`;
    vcolor=warn;
  }
  s+=`<text x="${M-11}" y="20" font-size="11" fill="${txt}" font-weight="700">${title}</text>`;
  s+=`<text x="${M-11}" y="${h-12}" font-size="9.5" fill="${vcolor}" font-weight="600">${verdict}</text>`;
  s+=`</svg>`;
  return s;
}

// ---------- D. target strip + baseline ----------
function drawFix(){
  const belt=css('--belt'),axis=css('--axis'),frame=css('--frame'),beam=css('--beam'),
        sensor=css('--sensor'),muted=css('--muted'),bad=css('--critical'),good=css('--good'),
        warn=css('--warning'),child=css('--child'),txt=css('--text-secondary');
  const PXs=6.4, M=54, HT=62;
  const w=WID*PXs+M*2+190, h=HT*PXs+M*2;
  const X=cm=>M+cm*PXs, Y=cm=>(M+HT*PXs)-cm*PXs;
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img">`;

  s+=`<rect x="${X(0)}" y="${Y(0)}" width="${WID*PXs}" height="8" fill="${belt}" stroke="${axis}"/>`;
  s+=`<rect x="${X(0)-14}" y="${Y(H_SENS+2)}" width="14" height="${(H_SENS+2)*PXs+8}" fill="${frame}"/>`;

  const zl=zLow(WID), zh=zHigh(WID), zm=zMid(WID);
  s+=`<polygon points="${X(0)},${Y(H_SENS)} ${X(WID)},${Y(zl)} ${X(WID)},${Y(zh)}"
       fill="${beam}" fill-opacity="0.16" stroke="${beam}" stroke-opacity="0.4"/>`;
  s+=`<circle cx="${X(0)}" cy="${Y(H_SENS)}" r="5" fill="${sensor}"/>`;

  // TARGET STRIP tại đúng độ cao tia
  s+=`<rect x="${X(WID)}" y="${Y(zh+3)}" width="11" height="${(zh+3-zl+3)*PXs}" fill="${good}" fill-opacity="0.55" stroke="${good}"/>`;
  s+=`<text x="${X(WID)+18}" y="${Y(zm)-16}" font-size="10.5" fill="${good}" font-weight="700">TARGET STRIP</text>`;
  s+=`<text x="${X(WID)+18}" y="${Y(zm)-4}" font-size="9.5" fill="${muted}">nhám, sáng, cao ${zl.toFixed(0)}–${zh.toFixed(0)}cm</text>`;
  s+=`<text x="${X(WID)+18}" y="${Y(zm)+8}" font-size="9.5" fill="${txt}" font-weight="600">baseline = ${slant(WID).toFixed(0)}cm</text>`;

  // trẻ
  const cw=12, cx0=WID/2-cw/2;
  s+=`<rect x="${X(cx0)}" y="${Y(60)}" width="${cw*PXs}" height="${60*PXs}" rx="5" fill="${child}" fill-opacity="0.38"/>`;
  s+=`<circle cx="${X(cx0)}" cy="${Y(zMid(cx0))}" r="4.5" fill="${warn}" stroke="#fff" stroke-width="1.3"/>`;
  s+=`<text x="${X(cx0)+9}" y="${Y(zMid(cx0))-7}" font-size="9.5" fill="${warn}" font-weight="700">có trẻ: ${slant(cx0).toFixed(0)}cm</text>`;
  s+=`<text x="${X(cx0)+9}" y="${Y(zMid(cx0))+4}" font-size="9" fill="${muted}">ngắn hơn baseline ${(slant(WID)-slant(cx0)).toFixed(0)}cm</text>`;

  // 3 trạng thái
  const bx=M-14, by=h-58;
  s+=`<text x="${bx}" y="${by}" font-size="10.5" fill="${txt}" font-weight="700">3 trạng thái phân biệt được:</text>`;
  s+=`<circle cx="${bx+7}" cy="${by+15}" r="4.5" fill="${good}"/>`;
  s+=`<text x="${bx+18}" y="${by+18.5}" font-size="9.5" fill="${muted}">đọc ≈ baseline (${slant(WID).toFixed(0)}cm) → băng trống, sensor OK</text>`;
  s+=`<circle cx="${bx+7}" cy="${by+31}" r="4.5" fill="${warn}"/>`;
  s+=`<text x="${bx+18}" y="${by+34.5}" font-size="9.5" fill="${muted}">đọc ngắn hơn baseline &gt;8cm → CÓ vật (trẻ) trong lát cắt</text>`;
  s+=`<circle cx="${bx+7}" cy="${by+47}" r="4.5" fill="${bad}"/>`;
  s+=`<text x="${bx+18}" y="${by+50.5}" font-size="9.5" fill="${muted}">đọc dài hơn baseline / mất tín hiệu → sensor lệch-hỏng → fail-closed</text>`;
  s+=`</svg>`;
  return s;
}

document.getElementById('fig3d').innerHTML = draw3D();
document.getElementById('figcross').innerHTML = drawCross();
document.getElementById('figC1').innerHTML = drawScenario(true);
document.getElementById('figC2').innerHTML = drawScenario(false);
document.getElementById('figD').innerHTML = drawFix();

document.getElementById('crossnotes').innerHTML =
  `<li>Tia thấp nhất chạm rìa xa ở <b>z=${zLow(WID).toFixed(0)}cm</b>, trục quang ở <b>z=${zMid(WID).toFixed(0)}cm</b>, tia cao nhất ở <b>z=${zHigh(WID).toFixed(0)}cm</b>.</li>`
  + `<li>Sensor trả về <b>vật gần nhất</b> trong nón. Trẻ đứng giữa băng → khóa vào mép gần ở d≈${(WID/2-6.5).toFixed(0)}cm, tức slant ≈ <b>${slant(WID/2-6.5).toFixed(0)}cm</b>.</li>`
  + `<li>Ngưỡng <code>900mm</code> ứng với khoảng cách ngang <b>${dAtThresh.toFixed(0)}cm</b> — <b>xa hơn cả bề rộng băng (63cm)</b>. Nghĩa là ngưỡng phủ trùm toàn bộ băng và còn vươn ra ngoài.</li>`
  + `<li>Vùng mù dưới tia thấp: cao 0→${zLow(WID).toFixed(0)}cm tăng dần theo d. Bàn chân ở rìa xa <b>không</b> bị thấy — với bạn đây là điểm tốt (giảm nhiễu sải chân).</li>`;

document.getElementById('cnotes').innerHTML =
  `<li><b>C1 (có khung/tay vịn cao):</b> tia chạm khung ở slant ≈ ${slant(WID-2).toFixed(0)}cm < 90cm → <code>ir_logic_is_detected()</code> luôn TRUE → cả 6 sensor báo "có người" 24/7 → <code>calculate_zone()</code> thấy DANGER role được kích hoạt → <b>zone kẹt DANGER vĩnh viễn, băng không bao giờ chạy được</b>.</li>`
  + `<li><b>C2 (đối diện trống):</b> tia bay ra ngoài, đọc tường phòng (vài mét) hoặc status 4. Lúc này không kẹt DANGER, nhưng <b>mất khả năng phân biệt "băng trống" với "sensor mù/lệch"</b> — cả hai đều cho "không detect". Mất một lớp fail-safe.</li>`
  + `<li>Điểm chung: <code>VL53_DETECT_MAX_MM = 900</code> được chọn cho hình học <i>chiếu dọc/chiếu xuống</i>, không còn đúng khi quét ngang qua băng rộng 63cm.</li>`;

document.getElementById('dnotes').innerHTML =
  `<li>Baseline mỗi sensor đo lúc boot (băng trống) ≈ <b>${slant(WID).toFixed(0)}cm</b> = 63/cos25°. Không hardcode — mỗi con tự học, tự bù sai số lắp và crosstalk của mount in 3D.</li>`
  + `<li>Detect theo <b>độ lệch</b>: <code>baseline - đọc_hiện_tại > 8cm</code> → có vật. Thay cho ngưỡng tuyệt đối 900mm.</li>`
  + `<li>Lợi ích fail-closed: đọc <i>dài hơn</i> baseline = target strip biến mất = sensor bị đụng lệch/hỏng → báo lỗi thay vì im lặng coi là "không có người".</li>`
  + `<li>Target strip nên <b>nhám, màu sáng, phẳng</b>, đặt vuông góc tia. Tránh bề mặt bóng/gương (phản xạ lệch hướng → mất tín hiệu).</li>`;
