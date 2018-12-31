<?php

namespace MediaWiki\Languages\Data;

include("ZhConversion.php");

echo "# -*- coding: utf-8 -*- \n";

function printDict($name, $dict) {
    echo "$name = {\n";
    foreach($dict as $key => $val) {
        echo "    u\"", $key, "\": u\"", $val, "\",\n";
    }
    echo "}\n";
    echo "\n";
}

printDict("zh2Hant", ZhConversion::$zh2Hant);
printDict("zh2Hans", ZhConversion::$zh2Hans);
printDict("zh2TW", ZhConversion::$zh2TW);
printDict("zh2HK", ZhConversion::$zh2HK);
printDict("zh2CN", ZhConversion::$zh2CN);
# printDict("zh2SG", ZhConversion::$zh2SG);
?>
