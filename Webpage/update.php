<?php
/**
 * Created by PhpStorm.
 * User: Oak
 * Date: 2018-05-22 (022)
 * Time: 오후 3:35
 */

include_once("config.php");
include_once(CHARTPHP_LIB_PATH . "/inc/chartphp_dist.php");

$p = new chartphp();

$totalData = 300;

$db_host = "localhost";
$db_user = "root";
$db_passwd = "root";
$db_name = "smartfarmdb";
$conn = mysqli_connect($db_host,$db_user,$db_passwd,$db_name);

$query = 'SELECT * FROM iot ORDER BY date DESC LIMIT '.$totalData;
$result = mysqli_query($conn, $query);
$chart_date = array();
$idx=0;
while($row = mysqli_fetch_array($result)) {
    $chart_date[0][$idx] = array($row['Date'], ((int)$row['Brightness'])/100);
    $chart_date[1][$idx] = array($row['Date'], $row['Temperature']);
    $idx++;
}

$p->data = $chart_date;
$p->chart_type = "line";

// Common Options
$p->title = "Lightness / Temperature Chart";
$p->xlabel = "Lightness / Temperature";
$p->ylabel = "Degrees";
$p->series_label = array("Lightness * 100","Temperature");

$out = $p->render('c1');
echo $out;
?>