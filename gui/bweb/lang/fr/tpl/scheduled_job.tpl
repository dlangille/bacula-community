<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> <TMPL_IF title><TMPL_VAR title><TMPL_ELSE>Prochains jobs </TMPL_IF></h1>
 </div>
 <div class='bodydiv'>
    <form name='form1' action='<TMPL_VAR cginame>?' method='GET'>
     <table id='id<TMPL_VAR ID>'></table>
     <button type="submit" class="bp" name='action' title='Lancer maintenant' value='run_job_mod'>
       <img src='/bweb/R.png' alt=''>  Lancer maintenant </button>
      <button type="submit" class="bp" name='action' title='D�sactiver' value='disable_job'>
       <img src='/bweb/inflag0.png' alt=''> D�sactiver </button>
       <button type="submit" onsubmit='document.form1.level.value="all"' class="bp" name='action' value='job' title='voir <TMPL_VAR Client> jobs'><img src='/bweb/zoom.png'>Voir les jobs</button>
<TMPL_IF wiki_url>
       <a id='wiki' href="<TMPL_VAR wiki_url>" title='Documentation'><img src='/bweb/doc.png' alt='Documentation'></a>Documentation
</TMPL_IF>
     <input type='hidden' name='pool' value=''>
     <input type='hidden' name='level' value=''>
     <input type='hidden' name='media' value=''>
     <input type='hidden' name='client' value=''>
    </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("Quand",
                       "Niveau",
	               "Type",
	               "Priorit�", 
                       "Nom",
                       "Volume",
	               "S�lection");

var data = new Array();
var chkbox;

var wiki_url <TMPL_IF wiki_url>='<TMPL_VAR wiki_url>'</TMPL_IF>;

<TMPL_LOOP list>
chkbox = document.createElement('INPUT');
chkbox.type  = 'radio';
chkbox.name = 'job';
chkbox.value = '<TMPL_VAR name>';
chkbox.onclick = function() { 
 document.form1.level.value = '<TMPL_VAR level>';
 document.form1.pool.value = '<TMPL_VAR pool>';
 document.form1.media.value = '<TMPL_VAR volume>';
 document.form1.client.value = '<TMPL_VAR client>';
 if (wiki_url) {
   document.getElementById('wiki').href=wiki_url + '<TMPL_VAR client>';
 }
} ;

data.push( new Array(
"<TMPL_VAR date>",    
"<TMPL_VAR level>",
"<TMPL_VAR type>",     
"<TMPL_VAR priority>",    
"<TMPL_VAR name>",      
"<TMPL_VAR volume>",
chkbox
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id<TMPL_VAR ID>",
 table_header: header,
 table_data: data,
 up_icon: up_icon,
 down_icon: down_icon,
 prev_icon: prev_icon,
 next_icon: next_icon,
 rew_icon:  rew_icon,
 fwd_icon:  fwd_icon,
// natural_compare: true,
 even_cell_color: even_cell_color,
 odd_cell_color: odd_cell_color, 
 header_color: header_color,
 page_nav: true,
 rows_per_page: rows_per_page,
// disable_sorting: new Array(6),
 padding: 3
}
);
</script>
