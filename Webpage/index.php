<!DOCTYPE html>
<html>
    <head>
        <link rel="stylesheet" href="lib/js/chartphp.css">
        <script src="lib/js/jquery.min.js"></script>
        <script src="lib/js/chartphp.js"></script>
        <meta charset="utf-8">
        <title>Humidity / Temperature Chart</title>
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <meta name="description" content="">
        <meta name="author" content="">
        <link rel="icon" type="image/png" href="http://www.chartphp.com/wp-content/uploads/favicon.png">
        <!-- Le styles -->
        <link href="bootstrap/css/bootstrap.min.css" rel="stylesheet">
        <style type="text/css">
            body {
                padding-top: 60px;
                padding-bottom: 0px;
            }
            .sidebar-nav {
                padding: 9px 0;
            }
            .nav
            {
                margin-bottom:10px;
            }
            .accordion-inner a {
                font-size: 13px;
                font-family:tahoma;
            }
            code
            {
                background: #FFF;
                border:0;
            }
        </style>


    </head>
    <body>
        <div class="container" id="chart">
            <script type="text/javascript">
                $(document).ready(function () {
                    $.ajax({ // 처음 웹페이지 로딩시 표시하기 위해
                        type: 'GET',
                        url: 'update.php',
                        success: function (result) {
                            $('#chart').html(result);
                        }
                    })
                    setInterval(function () {
                        $.ajax({
                            type: 'GET',
                            url: 'update.php',
                            success: function (result) {
                                $('#chart').html(result);
                            }
                        })
                    }, 1000);
                });
            </script>
        </div>
    </body>
</html>