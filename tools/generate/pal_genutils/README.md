Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
## Common utilies for python codegen scripts

To use these in other generator scripts, use and tweak the following snippet:

```python
import os,site; site.addsitedir(os.path.join(os.path.dirname(__file__), "../"))
from pal_genutils import foo
```
