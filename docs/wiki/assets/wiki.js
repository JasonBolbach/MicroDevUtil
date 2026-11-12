// sidebar filter + copy buttons; the pages work without it
(function(){
  var f=document.getElementById('navfilter');
  if(f)f.addEventListener('input',function(){var q=f.value.toLowerCase();
    document.querySelectorAll('nav.side a.item').forEach(function(a){a.style.display=a.textContent.toLowerCase().indexOf(q)<0?'none':''});});
  document.querySelectorAll('pre').forEach(function(p){var b=document.createElement('button');b.textContent='copy';
    b.onclick=function(){try{navigator.clipboard.writeText(p.innerText.replace(/copy$/,'').trim());b.textContent='copied';setTimeout(function(){b.textContent='copy'},1200)}catch(e){}};p.appendChild(b)});
  var c=document.querySelector('nav.side a.cur');if(c&&c.scrollIntoView)c.scrollIntoView({block:'center'});
})();
