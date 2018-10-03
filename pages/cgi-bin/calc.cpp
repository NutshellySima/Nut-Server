#include <algorithm>
#include <iostream>
#include <string>
#include <regex>
using namespace std;

string input;

int Calculate(int left, int right)
{
    int result = 0;
    for (int i = left; i <= right; ++i)
    {
        result += input[i] - '0';
        result *= 10;
    }
    result /= 10;
    return result;
}

int If_expr(int left, int right)
{
    int count = 0, flag = 0;
    for (int i = left; i <= right; ++i)
    {
        if (input[i] == '(')
        {
            ++count;
            flag = 1;
        }
        if (input[i] == ')')
            --count;
        if (count == 0 && (input[i] == '+' || input[i] == '-' || input[i] == '*' ||
                           input[i] == '/'))
            return 0;
    }
    if (flag == 1)
        return 1;
    return 0;
}

int R_Find_expr(int left, int right, char tofind)
{
    int count = 0;
    for (int i = right; i >= left; --i)
    {
        if (input[i] == ')')
            --count;
        if (input[i] == '(')
            ++count;
        if (count == 0 && (input[i] == tofind))
            return i;
    }
    return -1;
}

int Break_expr(int left, int right)
{
    if (If_expr(left, right))
        return Break_expr(left + 1, right - 1);
    int breaker = -1, breaker1 = -1, breaker2 = -1;
    breaker1 = R_Find_expr(left, right, '+');
    breaker2 = R_Find_expr(left, right, '-');
    breaker = max(breaker1, breaker2);
    if (breaker != -1)
    {
        if (breaker1 == breaker)
            return Break_expr(left, breaker1 - 1) + Break_expr(breaker1 + 1, right);
        if (breaker2 == breaker && breaker2 != left)
            return Break_expr(left, breaker2 - 1) - Break_expr(breaker2 + 1, right);
        if (breaker2 == breaker && breaker2 == left)
            return 0 - Break_expr(breaker2 + 1, right);
    }
    breaker1 = R_Find_expr(left, right, '*');
    breaker2 = R_Find_expr(left, right, '/');
    breaker = max(breaker1, breaker2);
    if (breaker != -1)
    {
        if (breaker1 == breaker)
            return Break_expr(left, breaker1 - 1) * Break_expr(breaker1 + 1, right);
        if (breaker2 == breaker)
            return Break_expr(left, breaker2 - 1) / Break_expr(breaker2 + 1, right);
    }
    return Calculate(left, right);
}

int main()
{
    cin >> input;
    if (input.empty())
    {
        string Content = R"(
<!doctype html>
<html lang="zh-cmn-Hans">

<head>
    <title>计算器</title>
    <meta charset="utf-8" />
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" type="text/css" media="screen" href="/styles.css" />
</head>

<body>
    <header>
        <h1>计算器</h1>
    </header>
    <div class="calc-container">
        <form action="/cgi-bin/calc.out" method="POST">
            <p>请输入公式：</p>
            <input type="text" name="num" />
            <button>计算</button>
        </form>
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
        cout << Content << endl;
        return 0;
    }
    input = input.substr(4);
    input = regex_replace(input, std::regex("%20"), " ");
    input = regex_replace(input, std::regex("%2B"), "+");
    input = regex_replace(input, std::regex("%2F"), "/");
    input = regex_replace(input, std::regex("%25"), "%");
    puts("Content-Type: text/html");
    puts("");
    string Result = R"(
<!doctype html>
<html lang="zh-cmn-Hans">

<head>
    <title>计算结果</title>
    <meta charset="utf-8" />
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" type="text/css" media="screen" href="/styles.css" />
</head>

<body>
    <header>
        <h1>计算结果</h1>
    </header>

    <div class="ip-container">
        <h2>%20</h2>
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

    int res;
    try
    {
        res = Break_expr(0, input.size() - 1);
        Result = regex_replace(Result, std::regex("%20"), to_string(res));
    }
    catch (exception &e)
    {
        Result = regex_replace(Result, std::regex("%20"), e.what());
    }

    cout << Result << endl;
    return 0;
}