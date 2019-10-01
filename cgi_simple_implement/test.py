# -*- coding:utf-8 -*-

import os

qs = os.getenv("query_str")
result = qs.upper()
print(result)
