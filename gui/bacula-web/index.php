<?php
/* 
+-------------------------------------------------------------------------+
| Copyright (C) 2004 Juan Luis Franc�s Jim�nez                            |
|                                                                         |
| This program is free software; you can redistribute it and/or           |
| modify it under the terms of the GNU General Public License             |
| as published by the Free Software Foundation; either version 2          |
| of the License, or (at your option) any later version.                  |
|                                                                         |
| This program is distributed in the hope that it will be useful,         |
| but WITHOUT ANY WARRANTY; without even the implied warranty of          |
| MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           |
| GNU General Public License for more details.                            |
+-------------------------------------------------------------------------+ 
*/
session_start();
require ("paths.php");
require($smarty_path."Smarty.class.php");
include "bweb.inc.php";

$smarty = new Smarty();     
$dbSql = new Bweb();

require("lang.php");

$mode = "";				

$smarty->compile_check = true;
$smarty->debugging = false;
$smarty->force_compile = true;

$smarty->template_dir = "./templates";
$smarty->compile_dir = "./templates_c";
$smarty->config_dir     = "./configs";

/*
$smarty->config_load("bacula.conf");                                                                                    // Load config file
$mode = $smarty->get_config_vars("mode");     
*/                                                                          // Lite o Extend?

// Getting mode from config file
$mode = $dbSql->get_config_param("mode");
if( $mode == false )
	$mode = "Lite";

$smarty->assign( "mode", $mode );

// Determine which template to show
$indexreport = $dbSql->get_config_param( "IndexReport" );

if( $indexreport == 0 ) {
	$smarty->assign( "last_report", "last_run_report.tpl" );
}else {
	$smarty->assign( "last_report", "report_select.tpl" );
}

// Assign to template catalogs number
$smarty->assign( "dbs", $dbSql->Get_Nb_Catalogs() );

//Assign dbs
/*
if ( count($dbSql->dbs) >1 ) {
  $smarty->assign("dbs", $dbSql->dbs);
  $smarty->assign("dbs_now", $_SESSION['DATABASE']);
}
*/

// generaldata.tpl & last_run_report.tpl ( Last 24 hours report )
$last24bytes = "";
$query = "";

/*$client = $dbSql->db_link->query("select count(*) from Client")
        or die ("Error query: 1");*/
  $totalfiles = $dbSql->db_link->query("select count(FilenameId) from Filename") or die ("Error query: 2");
  
  if ( PEAR::isError( $totalfiles ) ) {
	  die( "Unable to get Total Files information from catalog" . $totalfiles->getMessage() );
  }else {
	$tmp = $totalfiles->fetchRow();
	$smarty->assign('files_totales',$tmp[0]);
  }
  $totalfiles->free();
  
  switch( $dbSql->driver )
  {
	case 'mysql':
		$query = "select sum(JobBytes),count(*) from Job where Endtime <= NOW() and UNIX_TIMESTAMP(EndTime) > UNIX_TIMESTAMP(NOW())-86400";
	break;
	case 'pgsql':
		$query = "select sum(JobBytes),count(*) from Job where Endtime <= NOW() and EndTime > NOW() - 86400 * interval '1 second'";
	break;
	default:
		$query = "select sum(JobBytes),count(*) from Job where Endtime <= NOW() and UNIX_TIMESTAMP(EndTime) > UNIX_TIMESTAMP(NOW())-86400";
	break;
  }
  
  $last24bytes = $dbSql->db_link->query( $query ) or die ("Failed to get Total Job Bytes from catalog");
	
  if ( PEAR::isError( $last24bytes ) ) {
	die( "Unable to get Total Job Bytes from catalog" . $last24bytes->getMessage() );
  }else {
	$tmp = $last24bytes->fetchRow();
	//var_dump( $tmp );
	// Transfered bytes since last 24 hours
	$smarty->assign('bytes_totales', $dbSql->human_file_size( $tmp[0] ) );

	$smarty->assign('total_jobs', $tmp[1]);

	$last24bytes->free();		
  }
		
// Database size
$smarty->assign('database_size', $dbSql->GetDbSize());

// Total bytes stored
$bytes_stored = $dbSql->db_link->getOne("select SUM(VolBytes) from Media") or die ("Failed to get Total stored Bytes from catalog");
$smarty->assign('bytes_stored', $dbSql->human_file_size($bytes_stored) );

// Number of clients
$nb_clients = $dbSql->Get_Nb_Clients();
$smarty->assign('clientes_totales',$nb_clients["nb_client"] );

// Backup Job list for report.tpl and last_run_report.tpl
$smarty->assign( 'total_name_jobs', $dbSql->Get_BackupJob_Names() );

// Get volumes list (volumes.tpl)
$smarty->assign('pools',$dbSql->GetVolumeList() );

// Last 24 hours completed jobs number (last_run_report.tpl)
$completed_jobs = $dbSql->GetLastJobs();
$smarty->assign( 'completed_jobs', $completed_jobs['completed_jobs'] );

// Last 24 hours failed jobs number (last_run_report.tpl)
$failed_jobs = $dbSql->GetLastErrorJobs();
$smarty->assign( 'failed_jobs', $failed_jobs['failed_jobs'] );

// Last 24 hours elapsed time (last_run_report.tpl)
$smarty->assign( 'elapsed_jobs', $dbSql->Get_ElapsedTime_Job() );

// last_run_report.tpl
if ( $mode == "Lite" && $_GET['Full_popup'] == "yes" ) {
/*
        // Total Elapsed Time. Only for single Job.
        if ( $dbSql->driver == "mysql" )
          $ret = $dbSql->db_link->query("select UNIX_TIMESTAMP(EndTime)-UNIX_TIMESTAMP(StartTime) as elapsed from Job where EndTime <= NOW() and UNIX_TIMESTAMP(EndTime) > UNIX_TIMESTAMP(NOW())-84600")
                or die ("Error at row 110");
        if ( $dbSql->driver == "pgsql" )
          $ret = $dbSql->db_link->query("select EndTime - StartTime as elapsed from Job where EndTime <= NOW() and EndTime > NOW() - 84600 * interval '1 second'")
                or die ("Error at row 113");
        while ( $res = $ret->fetchRow() ) {
                if ( $TotalElapsed < 1000000000 )                                                                               // Temporal "workaround" ;) Fix later
                        $TotalElapsed += $res[0];
        }
        if ($TotalElapsed > 86400)                                                                                                      // More than 1 day!
                $TotalElapsed = gmstrftime("%d days %H:%M:%S", $TotalElapsed);
        else
                $TotalElapsed = gmstrftime("%H:%M:%S", $TotalElapsed);
        $smarty->assign('TotalElapsed',$TotalElapsed);
        $ret->free();
*/		
		
}
else if ($mode == "Full" || $_GET['Full_popup'] == "yes" ){
/*
        $tmp1 = array();
        if ( $dbSql->driver == "mysql")
                $query = "select SEC_TO_TIME( UNIX_TIMESTAMP(Job.EndTime)-UNIX_TIMESTAMP(Job.StartTime) )
                                as elapsed,Job.Name,Job.StartTime,Job.EndTime,Job.Level,Pool.Name,Job.JobStatus from Job 
                                LEFT JOIN Pool ON Job.PoolId=Pool.PoolId where EndTime <= NOW() and UNIX_TIMESTAMP(EndTime) >UNIX_TIMESTAMP(NOW())-86400 
                                order by elapsed ";                                                                                                     // Full report array
        if ( $dbSql->driver == "pgsql")
                $query = "select (Job.EndTime - Job.StartTime )
                                as elapsed,Job.Name,Job.StartTime,Job.EndTime,Job.Level,Pool.Name,Job.JobStatus from Job
                                LEFT JOIN Pool ON Job.PoolId=Pool.PoolId where EndTime <= NOW() and EndTime > NOW() - 86400 * interval '1 second'
                                order by elapsed ";
        $status = $dbSql->db_link->query($query)
                or die ("Error: query at row 138");
        while ( $tmp = $status->fetchRow() ) {
                $tdate = explode (":",$tmp[0]);
                if ( $tdate[0] > 300000 )                                                                                               // Temporal "workaround" ;) Fix later
                        $tmp[0] = "00:00:00";
                array_push($tmp1,$tmp);
        }
        
        $smarty->assign('clients',$tmp1);
*/
}

// Last 24 hours Job status graph
$data   = array();  
$status = array( 'completed', 'completed_errors', 'failed', 'waiting', 'created', 'running', 'error' );

foreach( $status as $job_status ) {
	array_push( $data, $dbSql->GetJobsStatistics( $job_status ) );
}

$graph = new BGraph( "graph.png" );
$graph->SetData( $data, 'pie', 'text-data-single' );
//$graph->SetTitle("Overall jobs status");
$graph->SetGraphSize( 400, 230 );
//$graph->SetColors( array('green', 'yellow','red','blue','white','green','red') );

$graph->Render();
$smarty->assign('graph_jobs', $graph->Get_Image_file() );
unset($graph);

// Pool and volumes graph
$data = array();
$graph = new BGraph( "graph1.png" );

$pools = $dbSql->Get_Pools_List();

foreach( $pools as $pool ) {
	array_push( $data, $dbSql->GetPoolsStatistics( $pool ) );
}

$graph->SetData( $data, 'pie', 'text-data-single' );
$graph->SetGraphSize( 400, 230 );


$graph->Render();
$smarty->assign('graph_pools', $graph->Get_Image_file() );

if ($_GET['Full_popup'] == "yes" || $_GET['pop_graph1'] == "yes" || $_GET['pop_graph2'] == "yes")
        $smarty->display('full_popup.tpl');
else
        $smarty->display('index.tpl');
?>
