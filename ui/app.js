'use strict';
/* Fully local library front-end. Does not invoke an emulator or change game data. */
(() => {
  const $ = (selector) => document.querySelector(selector);
  const $$ = (selector) => [...document.querySelectorAll(selector)];
  const PLATFORM = ['Alle Spiele', 'PS5', 'PS4', 'PS2', 'Switch', 'Xbox 360', 'Homebrew', 'Favoriten', 'Zuletzt gespielt'];
  const MARK = {'PS5':'PS5','PS4':'PS4','PS5/PS4':'PS','PS2':'PS2','Switch':'NS','Xbox 360':'X360','Homebrew':'HB','PS1':'PS1','PSP':'PSP'};
  const state = { tab:'Alle Spiele', query:'', sort:'title', games:[], roots:[], errors:[], active:0, detail:null, busy:false, server:false, latest:'', favorites:new Set(), recent:{} };
  const safeLoad = (key, fallback) => { try { const data=JSON.parse(localStorage.getItem(key)); return data ?? fallback; } catch { return fallback; } };
  const store = (key, value) => { try { localStorage.setItem(key, JSON.stringify(value)); } catch {} };
  state.favorites = new Set(safeLoad('ps5ul-favorites-v2', []));
  state.recent = safeLoad('ps5ul-recent-v2', {});
  state.customCovers = safeLoad('ps5ul-custom-covers-v2', {});
  let toastHandle = 0;
  const text = (tag, klass, str) => { const element=document.createElement(tag); if (klass) element.className=klass; element.textContent=str; return element; };
  const showToast = (message) => { const t=$('#toast'); t.textContent=message; t.classList.add('show'); clearTimeout(toastHandle); toastHandle=setTimeout(()=>t.classList.remove('show'), 2900); };
  const storedLibrary = () => safeLoad('ps5ul-import-v2', null);
  function installLibrary(input, persist=false) {
    if (!input || !Array.isArray(input.games)) { showToast('Keine gültige Bibliotheksdatei.'); return false; }
    const seen = new Set();
    state.games = input.games.filter((game) => game && typeof game==='object' && typeof game.title==='string' && typeof game.path==='string' && typeof game.platform==='string')
      .map((game) => ({...game, id: String(game.id || game.platform+'\0'+game.path), available:game.available!==false}))
      .filter((g) => !seen.has(g.id) && seen.add(g.id));
    state.roots = Array.isArray(input.unavailable_paths) ? input.unavailable_paths : [];
    state.errors = Array.isArray(input.errors) ? input.errors : [];
    state.latest = String(input.generated_at||'');
    if (persist) store('ps5ul-import-v2', input);
    redraw();
    return true;
  }
  const matchesTab = (game) => state.tab==='Alle Spiele' || (state.tab==='Favoriten' && state.favorites.has(game.id)) || (state.tab==='Zuletzt gespielt' && state.recent[game.id]) || game.platform===state.tab || false;
  function visible() {
    const search=state.query.trim().toLocaleLowerCase('de');
    const games=state.games.filter(game=>matchesTab(game) && (!search || [game.title,game.platform,game.title_id,game.path].some(s=>String(s||'').toLocaleLowerCase('de').includes(search))));
    return games.sort((a,b) => {
      if (state.sort==='recent') return (state.recent[b.id]||0)-(state.recent[a.id]||0) || a.title.localeCompare(b.title,'de');
      if (state.sort==='platform') return a.platform.localeCompare(b.platform,'de') || a.title.localeCompare(b.title,'de');
      return a.title.localeCompare(b.title,'de');
    });
  }
  function makePoster(game, custom=false) {
    const poster=text('div', custom ? 'detail-cover' : 'poster', '');
    if (state.customCovers[game.id]) {
      const img=document.createElement('img');img.src=state.customCovers[game.id];img.alt='Manuelles Cover: '+game.title;img.loading='lazy';
      img.onerror=()=>{img.remove();fillPlaceholder(poster,game,custom);};poster.append(img);
    } else if (game.cover_path && state.server) {
      const img=document.createElement('img');
      img.src='/api/cover/'+encodeURIComponent(game.id);
      img.alt='Cover: '+game.title;
      img.loading='lazy';
      img.onerror=()=>{img.remove();fillPlaceholder(poster,game,custom);};
      poster.append(img);
    } else fillPlaceholder(poster,game,custom);
    if (!custom) {
      poster.append(text('div','poster-overlay',''));
      poster.append(text('span','platform-tag',game.platform));
      if (state.favorites.has(game.id)) poster.append(text('span','favorite-mark','★'));
      if (!game.available) poster.append(text('span','offline-mark','DATENTRÄGER OFFLINE'));
    }
    return poster;
  }
  function fillPlaceholder(poster,game,custom=false) {
    poster.textContent='';
    if (custom) {poster.textContent=MARK[game.platform]||'GAME';return;}
    const place=text('div','no-cover','');
    place.append(text('div','poster-platform-mark',MARK[game.platform]||'GAME'));
    place.append(text('span','cover-missing','Kein lokales Cover'));
    poster.append(place);
  }
  function renderGames() {
    const games=visible();
    const view=$('#library'); view.replaceChildren();
    $('#section-name').textContent = state.tab==='Alle Spiele' ? 'Meine Bibliothek' : state.tab;
    $('#result-count').textContent=`${games.length} ${games.length===1?'Spiel':'Spiele'}`;
    const fragment=document.createDocumentFragment();
    games.forEach((game,index)=>{
      const tile=document.createElement('button');
      tile.type='button'; tile.className='tile'+(!game.available?' tile-offline':'');
      tile.setAttribute('data-platform',game.platform);tile.setAttribute('aria-label',`${game.title} – ${game.platform}${!game.available?' (offline)':''}`);
      tile.dataset.gameId=game.id;
      tile.append(makePoster(game));tile.append(text('h3','',game.title));tile.append(text('small','',game.platform+' · '+(game.format||'Ordner')));
      tile.addEventListener('click',()=>openGame(game));
      tile.addEventListener('focus',()=>{state.active=index; $$('.tile.selected').forEach(el=>el.classList.remove('selected'));tile.classList.add('selected');});
      fragment.append(tile);
    });
    view.append(fragment);
    $('#empty').hidden=games.length!==0;
    $('#library').hidden=games.length===0;
    $('#empty-title').textContent = state.games.length ? 'Keine passenden Spiele gefunden' : 'Noch keine Spiele erkannt';
    $('#empty-body').textContent = state.games.length ? 'Wechsle den Plattformfilter oder passe deine Suche an.' : 'Prüfe deine Verzeichnisse in den Einstellungen und aktualisiere die Bibliothek.';
    state.active=Math.min(state.active,Math.max(0,games.length-1));
  }
  function redraw() {
    $$('.tab').forEach(button=>button.classList.toggle('active',button.dataset.tab===state.tab));
    const available=state.games.filter(g=>g.available).length;
    const platforms=new Set(state.games.map(g=>g.platform));
    $('#hero-count').textContent=state.games.length;
    $('#hero-label').textContent=state.games.length===1 ? 'Spiel an einem Ort.' : 'Spiele an einem Ort.';
    $('#stat-available').textContent=available;
    $('#stat-platforms').textContent=platforms.size;
    $('#stat-offline').textContent=state.games.length-available;
    $('#hero-caption').textContent=state.latest ? 'Letzte Erkennung: '+formatDate(state.latest) : 'Alle Plattformen vereint – unabhängig vom Speicherort.';
    $('#footer-status').textContent=state.busy?'BIBLIOTHEK WIRD GELADEN':'BIBLIOTHEK BEREIT';
    const notice=$('#notice');
    if(state.roots.length || state.errors.length){notice.hidden=false;notice.textContent=`${state.roots.length} nicht erreichbare Spieleordner · ${state.errors.length} Lesefehler. Details unter Einstellungen.`;}
    else notice.hidden=true;
    renderGames();
  }
  function formatDate(raw) {const date=new Date(raw);return Number.isNaN(date.valueOf())?'unbekannt':date.toLocaleString('de-AT',{dateStyle:'medium',timeStyle:'short'});}
  function setTab(tab) {if(!PLATFORM.includes(tab))return;state.tab=tab;state.active=0;redraw();$('#tabs').querySelector('.tab.active')?.scrollIntoView({block:'nearest',inline:'nearest'});}
  function toggleFav(game) {
    if(state.favorites.has(game.id))state.favorites.delete(game.id);else state.favorites.add(game.id);
    store('ps5ul-favorites-v2',[...state.favorites]);redraw();
    if($('#game-dialog').open) openGame(game,true);
  }
  function openGame(game, refresh=false) {
    const parent=$('#detail-content');parent.replaceChildren();
    parent.append(text('div','eyebrow',game.platform+' · '+(game.available?'SPIEL VERFÜGBAR':'NICHT VERFÜGBAR')));
    const layout=text('div','detail-layout','');
    layout.append(makePoster(game,true));
    const info=text('div','detail-info','');
    info.append(text('h2','',game.title));
    const meta=text('div','meta','');
    meta.append(text('div','',`Format: ${game.format||'Unbekannt'}`));
    if(game.title_id)meta.append(text('div','',`Title-ID: ${game.title_id}`));
    meta.append(text('div','',`Speicherort: ${game.path}`));
    meta.append(text('div','',`Metadaten: ${game.metadata_source||'nicht vorhanden'}`));
    info.append(meta);
    const actions=text('div','detail-buttons','');
    const launch=text('button','primary-action','SPIEL STARTEN');launch.disabled=true;
    launch.title='Direktstart ist auf Firmware 13.60 noch nicht als integrierte Funktion verifiziert.';
    const fav=text('button','subtle-action',state.favorites.has(game.id)?'★ AUS FAVORITEN ENTFERNEN':'☆ ZU FAVORITEN');
    fav.addEventListener('click',()=>toggleFav(game));
    const coverButton=text('button','subtle-action','▧ COVER ÄNDERN');
    coverButton.addEventListener('click',()=>$('#cover-file').click());
    actions.append(launch,fav,coverButton);
    if(state.customCovers[game.id]) { const clear=text('button','subtle-action','COVER ZURÜCKSETZEN');clear.addEventListener('click',()=>{delete state.customCovers[game.id];store('ps5ul-custom-covers-v2',state.customCovers);redraw();openGame(game,true);});actions.append(clear); }
    info.append(actions);
    info.append(text('p','muted-note',!game.available?'Der Datenträger mit diesem Spiel ist derzeit nicht erreichbar.':'Direktstart derzeit nicht unterstützt: Die plattformspezifische Übergabe ist in dieser Version noch nicht implementiert. Es wird kein erfolgreicher Spielstart vorgetäuscht.'));
    layout.append(info);parent.append(layout);state.detail=game;
    if(!refresh) $('#game-dialog').showModal();
  }
  function renderSettings() {
    const view=$('#settings-body');view.replaceChildren();
    view.append(text('h3','','Nicht erreichbare Spieleordner'));
    if(!state.roots.length)view.append(text('p','meta','Keine fehlenden Ordner gemeldet.'));
    state.roots.forEach(root=>{const r=text('div','root-row','');r.append(text('span','',String(root.path||'')),text('b','off','OFFLINE · '+String(root.platform||'')));view.append(r);});
    view.append(text('h3','','Bibliothek / Status'));
    const status=text('div','root-row','');status.append(text('span','',`${state.games.length} Spiele · ${state.errors.length} Lesefehler`),text('b','',state.server?'HOST-SCAN VERFÜGBAR':'OFFLINE-MODUS'));view.append(status);
    if(state.errors.length){view.append(text('h3','','Lesefehler'));state.errors.slice(0,20).forEach(error=>{const r=text('div','root-row','');r.append(text('span','',String(error.path||'')),text('b','off',String(error.error||'')));view.append(r);});}
    view.append(text('h3','','Konfiguration ändern'));
    view.append(text('p','meta','Passe config/paths.json außerhalb der Oberfläche an, um weitere USB-Mountpoints oder Spielordner zu ergänzen. Im Entwicklungsmodus anschließend „Bibliothek aktualisieren“ auswählen.'));
  }
  function download(filename,data,mime='application/json') {const blob=new Blob([data],{type:mime});const url=URL.createObjectURL(blob);const link=document.createElement('a');link.href=url;link.download=filename;link.click();setTimeout(()=>URL.revokeObjectURL(url),1500);}
  async function refresh() {
    if(state.busy)return;
    if(!state.server) {$('#import-file').click();showToast('Offline: Bitte aktualisierte Bibliotheks-JSON auswählen.');return;}
    state.busy=true;$('#refresh').disabled=true;redraw();
    try {const response=await fetch('/api/refresh',{method:'POST',headers:{'Content-Type':'application/json'},body:'{}'});if(!response.ok)throw new Error('Scan fehlgeschlagen');const library=await response.json();installLibrary(library);showToast(`${state.games.length} Spiele erkannt.`);}
    catch(error) {showToast('Bibliothek konnte nicht aktualisiert werden: '+error.message);}
    finally {state.busy=false;$('#refresh').disabled=false;redraw();}
  }
  async function probeServer() {
    if(location.protocol!=='http:' && location.protocol!=='https:')return;
    try {const res=await fetch('/api/library',{cache:'no-store'});if(!res.ok)return;state.server=true;$('#mode-pill').textContent='HOST-BIBLIOTHEK';installLibrary(await res.json());} catch {}
  }
  function focusGame(index) {
    const tiles=$$('.tile');if(!tiles.length)return;
    state.active=Math.max(0,Math.min(index,tiles.length-1));tiles[state.active].focus({preventScroll:true});tiles[state.active].scrollIntoView({block:'nearest',inline:'nearest',behavior:'smooth'});
  }
  function navigate(dx,dy) {
    if($('#game-dialog').open||$('#settings-dialog').open)return;
    const first=$('.tile');if(!first)return;
    const columns=Math.max(1,Math.round($('#library').clientWidth / (first.getBoundingClientRect().width + 20)));
    focusGame(state.active+dx+dy*columns);
  }
  function closeActive() {if($('#game-dialog').open)$('#game-dialog').close();else if($('#settings-dialog').open)$('#settings-dialog').close();else if($('#search').value){$('#search').value='';state.query='';redraw();}}
  function handleAction(action) {
    if(action==='back'){closeActive();return;}
    if(action==='settings'){renderSettings();if(!$('#settings-dialog').open)$('#settings-dialog').showModal();return;}
    if($('#game-dialog').open||$('#settings-dialog').open){if(action==='accept' && $('#game-dialog').open){showToast('Direktstart ist noch nicht verifiziert.');}return;}
    if(action==='search'){$('#search').focus();return;}
    if(action==='nextTab'||action==='prevTab'){const next=PLATFORM.indexOf(state.tab)+(action==='nextTab'?1:-1);setTab(PLATFORM[(next+PLATFORM.length)%PLATFORM.length]);return;}
    if(action==='accept'){const game=visible()[state.active];if(game)openGame(game);return;}
    if(action==='up')navigate(0,-1);if(action==='down')navigate(0,1);if(action==='left')navigate(-1,0);if(action==='right')navigate(1,0);
  }
  // Controller API uses browser gamepad polling in the host/test UI, not a native PS5 input claim.
  let previousKeys=new Set(), lastMove=0;
  function checkGamepad() {
    if(typeof navigator.getGamepads==='function') {
      const pad=[...navigator.getGamepads()].find(p=>p && p.connected);
      if(pad){
        const current=new Set();const button=(i,key)=>{if(pad.buttons[i]?.pressed)current.add(key);};
        button(0,'accept');button(1,'back');button(3,'search');button(4,'prevTab');button(5,'nextTab');button(9,'settings');
        button(12,'up');button(13,'down');button(14,'left');button(15,'right');
        if(pad.axes[0]<-.53)current.add('left');if(pad.axes[0]>.53)current.add('right');
        if(pad.axes[1]<-.53)current.add('up');if(pad.axes[1]>.53)current.add('down');
        const now=performance.now();
        for(const key of current){if(!previousKeys.has(key)||(['up','down','left','right'].includes(key)&&now-lastMove>170)){handleAction(key);if(['up','down','left','right'].includes(key))lastMove=now;}}
        previousKeys=current;
      }else previousKeys=new Set();
    }
    requestAnimationFrame(checkGamepad);
  }
  function boot() {
    installLibrary(storedLibrary() || window.PS5UL_LIBRARY || {games:[],unavailable_paths:[]});
    $('#clock').textContent=new Date().toLocaleTimeString('de-AT',{hour:'2-digit',minute:'2-digit'});
    setInterval(()=>{$('#clock').textContent=new Date().toLocaleTimeString('de-AT',{hour:'2-digit',minute:'2-digit'});},30000);
    $$('.tab').forEach(tab=>tab.addEventListener('click',()=>setTab(tab.dataset.tab)));
    $('#search').addEventListener('input',event=>{state.query=event.target.value;state.active=0;renderGames();});
    $('#sort').addEventListener('change',event=>{state.sort=event.target.value;renderGames();});
    $('#refresh').addEventListener('click',refresh);$('#empty-refresh').addEventListener('click',refresh);
    $('#settings-button').addEventListener('click',()=>handleAction('settings'));
    $$('[data-close]').forEach(button=>button.addEventListener('click',()=>$('#'+button.dataset.close).close()));
    $('#import').addEventListener('click',()=>$('#import-file').click());
    $('#cover-file').addEventListener('change',(event)=>{
      const file=event.target.files[0], game=state.detail; if(!file || !game)return;
      if(!['image/png','image/jpeg','image/webp'].includes(file.type)||file.size>1_500_000){showToast('Bitte PNG/JPG/WebP unter 1,5 MB verwenden.');event.target.value='';return;}
      const reader=new FileReader();reader.onload=()=>{
        const next={...state.customCovers,[game.id]:reader.result};
        try{localStorage.setItem('ps5ul-custom-covers-v2',JSON.stringify(next));state.customCovers=next;redraw();openGame(game,true);showToast('Cover lokal gespeichert.');}
        catch{showToast('Browserspeicher voll. Bitte kleineres Cover verwenden.');}
      };reader.readAsDataURL(file);event.target.value='';
    });
    $('#import-file').addEventListener('change',async(event)=>{const file=event.target.files[0];if(!file)return;try{const parsed=JSON.parse(await file.text());if(installLibrary(parsed,true)){showToast('Bibliothek erfolgreich importiert.');$('#settings-dialog').close();}}catch{showToast('Datei ist kein gültiges JSON.');}event.target.value='';});
    $('#export').addEventListener('click',()=>download('ps5-launcher-library.json',JSON.stringify({schema:2,generated_at:state.latest,games:state.games,unavailable_paths:state.roots,errors:state.errors},null,2)));
    $('#config-download').addEventListener('click',()=>download('paths.template.json',JSON.stringify({version:2,max_depth:6,paths:{'PS5/PS4':['/data/etaHEN/games'],PS2:['/data/PCSX2/games','/mnt/usb1/PS5SX2/games'],Switch:['/data/prosperoeden/roms'], 'Xbox 360':['/data/xbox360','/data/homebrew/PPSA50011/assets/roms']},custom_paths:[]},null,2)));
    document.addEventListener('keydown',event=>{
      if(['INPUT','TEXTAREA','SELECT'].includes(document.activeElement?.tagName)){if(event.key==='Escape'){document.activeElement.blur();closeActive();}return;}
      const map={ArrowUp:'up',ArrowDown:'down',ArrowLeft:'left',ArrowRight:'right',Enter:'accept',Escape:'back',Backspace:'back',q:'prevTab',e:'nextTab',f:'search',o:'settings'};
      const action=map[event.key];if(action){event.preventDefault();handleAction(action);}
    });
    probeServer();requestAnimationFrame(checkGamepad);
  }
  boot();
})();
