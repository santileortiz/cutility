/*
 * Copyright (C) 2019 Santiago León O.
 */

void replace_test (struct test_ctx_t *t, char *test_name, mem_pool_t *pool,
                   char *str, char *find, char *replace, char *expected, int expected_replacements)
{
    bool success = true;
    test_push (t, "%s", test_name);

    int replacements = 0xC0FEE; // initialize with garbage...
    char *res = cstr_dupreplace (pool, str, find, replace, &replacements);
    if (strcmp (res, expected) != 0 || replacements != expected_replacements) {
        str_cat_printf (t->error, "String replacement:\n");
        str_cat_printf (t->error, " src: '%s'\n", str);
        str_cat_printf (t->error, " find: '%s'\n", find);
        str_cat_printf (t->error, " replace: '%s'\n\n", replace);
        str_cat_printf (t->error, "Expected: '%s'\n     got: '%s'\n", expected, res);
        str_cat_printf (t->error, "Expected replacements: %d\n                  got: %d\n", expected_replacements, replacements);
        success = false;
    }

    test_pop (t, success);
}

void string_tests (struct test_ctx_t *t)
{
    test_push (t, "String");

    {
        string_t str = {0};
        test_push (t, "set/cat print-like");
        str_set_printf (&str, "%d, and '%s', plus %c", 120, "FOO", 'o');
        str_cat_printf (&str, ", some appended text %d", 44);
        if (strcmp (str_data(&str), "120, and 'FOO', plus o, some appended text 44") != 0) {
            test_pop (t, false);
        } else {
            test_pop (t, true);
        }
        str_free (&str);
    }

    {
        mem_pool_t pool = {0};

        replace_test (t, "Simple replacement", &pool,
                      "some X random string X",
                      "X",
                      "random",
                      "some random random string random", 2);

        replace_test (t, "Replace to empty string (delete)", &pool,
                      "some teDELETEMEst with deletemeDELETEME",
                      "DELETEME",
                      "",
                      "some test with deleteme", 2);

        replace_test (t, "Not found", &pool,
                      "a string",
                      "other stuff",
                      "",
                      "a string", 0);


        mem_pool_destroy (&pool);

        test_push (t, "cstr_rstrip");
        {
            char str[] = "  Hey there    ";
            cstr_rstrip (str);
            test_str (t, "normal usage", str, "  Hey there");
        }

        {
            char str[] = "    ";
            cstr_rstrip (str);
            test_str (t, "all spaces", str, "");
        }

        {
            char str[] = "";
            cstr_rstrip (str);
            test_str (t, "empty string", str, "");
        }

        {
            char str[] = "something";
            cstr_rstrip (str);
            test_str (t, "no spaces", str, "something");
        }
        test_pop_parent (t);

        {
            test_push (t, "str_strip");

            string_t str = {0};

            str_set(&str, "  Hey there    ");
            str_strip (&str);
            test_str (t, "normal usage", str_data(&str), "Hey there");

            str_set(&str, "    ");
            str_strip (&str);
            test_str (t, "all spaces", str_data(&str), "");

            str_set(&str, "");
            str_strip (&str);
            test_str (t, "empty string", str_data(&str), "");

            str_set(&str, "something");
            str_strip (&str);
            test_str (t, "no spaces", str_data(&str), "something");

            str_free(&str);
            test_pop_parent (t);
        }
    }

    {
        test_push (t, "cstr_find_open_parenthesis");

        size_t pos;
        int count;

        test_push (t, "Common Case");
        char *str1 = "Some text Start a ((((()))())()) contained (paren) thesis (() and split close) more text (A (and parenthesis) within((((()))())())x () blah";
        bool ret = cstr_find_open_parenthesis (str1, &pos, &count);
        test_int (t, "ret", ret, true);
        test_int (t, "pos", pos, 89);
        test_int (t, "count", count, 1);
        test_pop_parent (t);


        test_push (t, "count==2");
        char *str2 = "Some text Start a ((((()))())()) contained (paren) thesis ((() and split close) more text (A (and parenthesis) within((((()))())())x () blah";
        ret = cstr_find_open_parenthesis (str2, &pos, &count);
        test_int (t, "ret", ret, true);
        test_int (t, "pos", pos, 58);
        test_int (t, "count", count, 2);
        test_pop_parent (t);

        test_push (t, "No open parenthesis");
        char *str3 = "Some text Start a ((((()))())()) contained (paren) thesis ((() and split close) more text (A (and parenthesis) within((((()))())())x () blah))";
        ret = cstr_find_open_parenthesis (str3, &pos, &count);
        test_int (t, "ret", ret, false);
        test_int (t, "pos", pos, 0);
        test_int (t, "count", count, 0);
        test_pop_parent (t);

        test_pop_parent (t);

        test_push (t, "cstr_find_close_parenthesis");

        test_push (t, "Common Case");
        ret = cstr_find_close_parenthesis("some text ()) (bla) lorem", 1, &pos);
        test_int (t, "ret", ret, true);
        test_int (t, "pos", pos, 12);
        test_pop_parent (t);

        test_push (t, "Common Case");
        char *str4 = "some ((d((d(d))s)(a))(d))text ()) ((d((d(d))s)(a))(d))) ()(()(()((())))) lorem";
        ret = cstr_find_close_parenthesis(str4, 2, &pos);
        test_int (t, "ret", ret, true);
        test_int (t, "pos", pos, 54);
        test_pop_parent (t);

        test_push (t, "No close parenthesis");
        char *str5 = "some text () (bla) lorem";
        ret = cstr_find_close_parenthesis(str5, 1, &pos);
        test_int (t, "ret", ret, false);
        test_int (t, "pos", pos, strlen(str5));
        test_pop_parent (t);

        test_push (t, "Zero count");
        ret = cstr_find_close_parenthesis("some ((d((d(d))s)(a))(d))text ()) ((d((d(d))s)(a))(d))) ()(()(()((())))) lorem", 0, &pos);
        test_int (t, "ret", ret, false);
        test_int (t, "pos", pos, 0);
        test_pop_parent (t);

        test_pop_parent (t);
    }

    test_pop_parent (t);
}
