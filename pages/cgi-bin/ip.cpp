#include <iostream>
#include <string>
#include <regex>
#include <stdlib.h>
using namespace std;

int main()
{
    puts("Content-Type: text/html");

    string Result = R"(
<!doctype html>
<html lang="zh-cmn-Hans">

<head>
    <title>IP</title>
    <meta charset="utf-8" />
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" type="text/css" media="screen" href="/styles.css" />
</head>

<body>
    <header>
        <h1>IP</h1>
    </header>

    <div class="ip-container">
        <p>%20</p>
    </div>
    <footer>
        <div class="footnote">
            <a href="https://github.com/NutshellySima" target="_blank" rel="noopener">
                <img src="/links-and-images/github.png" width="35" />
            </a>
            <p>Copyright © 2018 Chijun Sima <br /><em>Powered by <strong>Nut-Server</strong></em></p>
        </div>
    </footer>


</body>

</html>
    )";

    Result = regex_replace(Result, std::regex("%20"), getenv ("REMOTE_ADDR"));
    cout<<"Content-length: " + std::to_string(Result.size()) + "\r\n";
    printf("\r\n");
    cout << Result << endl;
    return 0;
}