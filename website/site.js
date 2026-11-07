'use strict';
(() => {
  const copy=JSON.parse(document.querySelector('#site-content').textContent);
  const config=JSON.parse(document.querySelector('#site-config').textContent);
  // Preserve historical links which opened the old root documentation reader.
  if(location.hash.startsWith('#/')){location.replace(config.documentation+location.search+location.hash);return;}
  document.documentElement.classList.add('js');
  const selector=document.querySelector('.language');
  const nav=document.querySelector('.primary');
  const menu=document.querySelector('.menu-toggle');
  const dialog=document.querySelector('#image-dialog');
  const initial=new URL(location.href);
  let language=initial.searchParams.get('lang')==='fr'?'fr':'en';
  let activeImage=null,previousFocus=null,previousScroll=null;
  const text=key=>copy[key][language];
  const imageButtons=()=>[...document.querySelectorAll('[data-zoom]')];
  function updateDialog(){
    if(!activeImage)return;
    const key=activeImage.dataset.zoom;
    const original=activeImage.querySelector('img');
    dialog.querySelector('img').src=original.currentSrc||original.src;
    dialog.querySelector('img').alt=text(key+'.alt');
    dialog.querySelector('#image-caption').textContent=text(key+'.caption');
    dialog.querySelector('.image-counter').textContent=`${imageButtons().indexOf(activeImage)+1} / ${imageButtons().length}`;
  }
  function address(next){const url=new URL(location.href);url.searchParams.set('lang',next);try{history.replaceState(null,'',url);}catch{/* Some file-origin hosts restrict history; content still changes locally. */}}
  function translate(next,writeAddress=false){
    language=next==='fr'?'fr':'en';document.documentElement.lang=language;selector.value=language;
    document.title=text('title');
    document.querySelector('meta[name="description"]').content=text('description');
    document.querySelector('meta[property="og:title"]').content=text('title');
    document.querySelector('meta[property="og:description"]').content=text('description');
    for(const node of document.querySelectorAll('[data-i18n]'))node.textContent=text(node.dataset.i18n);
    for(const node of document.querySelectorAll('[data-i18n-alt]'))node.alt=text(node.dataset.i18nAlt);
    for(const node of document.querySelectorAll('[data-i18n-aria]'))node.setAttribute('aria-label',text(node.dataset.i18nAria));
    for(const link of document.querySelectorAll('[data-doc]'))link.href=`${config.documentation}#/${language}/${link.dataset.doc}`;
    if(writeAddress)address(language);if(dialog.open)updateDialog();
  }
  selector.addEventListener('change',()=>translate(selector.value,true));
  function closeMenu(restore=false){nav.dataset.open='false';menu.setAttribute('aria-expanded','false');if(restore)menu.focus();}
  menu.addEventListener('click',()=>{const open=menu.getAttribute('aria-expanded')!=='true';menu.setAttribute('aria-expanded',String(open));nav.dataset.open=String(open);});
  nav.addEventListener('click',event=>{if(event.target.closest('a'))closeMenu();});
  document.addEventListener('keydown',event=>{if(event.key==='Escape'&&menu.getAttribute('aria-expanded')==='true')closeMenu(true);});
  window.matchMedia('(min-width:801px)').addEventListener('change',event=>{if(event.matches)closeMenu();});
  for(const gallery of document.querySelectorAll('[data-gallery]')){
    const tabs=[...gallery.querySelectorAll('[role="tab"]')];const panel=gallery.querySelector('[role="tabpanel"]');
    function choose(tab){
      for(const other of tabs){other.setAttribute('aria-selected',String(other===tab));other.tabIndex=other===tab?0:-1;}
      panel.setAttribute('aria-labelledby',tab.id);
      const key=tab.dataset.galleryKey,button=panel.querySelector('[data-zoom]'),image=button.querySelector('img');
      button.dataset.zoom=key;button.dataset.i18nAria=key+'.zoom';image.src=config.assets[tab.dataset.galleryFile];image.dataset.i18nAlt=key+'.alt';
      panel.querySelector('figcaption').dataset.i18n=key+'.caption';translate(language);
    }
    for(const tab of tabs){tab.addEventListener('click',()=>choose(tab));tab.addEventListener('keydown',event=>{
      if(!['ArrowLeft','ArrowRight','Home','End'].includes(event.key))return;event.preventDefault();
      const index=event.key==='Home'?0:event.key==='End'?tabs.length-1:(tabs.indexOf(tab)+(event.key==='ArrowRight'?1:-1)+tabs.length)%tabs.length;
      choose(tabs[index]);tabs[index].focus();
    });}
  }
  for(const button of imageButtons())button.addEventListener('click',()=>{
    previousFocus=button;previousScroll=[scrollX,scrollY];activeImage=button;updateDialog();
    document.body.classList.add('modal-open');dialog.showModal();dialog.querySelector('[data-close]').focus();
  });
  const close=()=>{if(dialog.open)dialog.close();};
  dialog.querySelector('[data-close]').addEventListener('click',close);
  function move(direction){const buttons=imageButtons();activeImage=buttons[(buttons.indexOf(activeImage)+direction+buttons.length)%buttons.length];updateDialog();}
  dialog.querySelector('[data-previous]').addEventListener('click',()=>move(-1));
  dialog.querySelector('[data-next]').addEventListener('click',()=>move(1));
  dialog.addEventListener('keydown',event=>{
    if(event.key==='ArrowLeft'||event.key==='ArrowRight'){event.preventDefault();move(event.key==='ArrowRight'?1:-1);}
    if(event.key==='Tab'){
      const buttons=[...dialog.querySelectorAll('button')],first=buttons[0],last=buttons.at(-1);
      if(event.shiftKey&&document.activeElement===first){event.preventDefault();last.focus();}
      else if(!event.shiftKey&&document.activeElement===last){event.preventDefault();first.focus();}
    }
  });
  dialog.addEventListener('click',event=>{if(event.target===dialog)close();});
  dialog.addEventListener('close',()=>{
    document.body.classList.remove('modal-open');if(previousScroll)window.scrollTo({left:previousScroll[0],top:previousScroll[1],behavior:'instant'});
    if(previousFocus?.isConnected)previousFocus.focus({preventScroll:true});activeImage=null;
  });
  const chapters=[...document.querySelectorAll('.chapter-inner a')];
  if('IntersectionObserver' in window){const observer=new IntersectionObserver(entries=>{for(const entry of entries)if(entry.isIntersecting)for(const link of chapters){if(link.hash===`#${entry.target.id}`)link.setAttribute('aria-current','true');else link.removeAttribute('aria-current');}},{rootMargin:'-20% 0px -55% 0px'});for(const link of chapters){const section=document.querySelector(link.hash);if(section)observer.observe(section);}}
  translate(language,initial.searchParams.has('lang')&&!['en','fr'].includes(initial.searchParams.get('lang')));
})();
