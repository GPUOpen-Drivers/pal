
## Rapidjson Overview

[RapidJSON](http://rapidjson.org/) is a header-only library designed to be
integrated into other projects (clients) with low friction. It and its
components are licensed under MIT, BSD, and the JSON license. These are detailed
in the `license.txt` file.

Standard rapidjson usage is to copy the files somewhere that the your build
system can `#include` them. It is a header-only library, so there's nothing to
compile and link against.

## tl:dr

Find the commands in the next section and run them from cmd.exe in
`//depot/stg/gpuopen/gpuopen/third_party`.

## Importing rapidjson

Here's a quick overview of the process:

1. Use git to clone the version we want
2. Copy the files we want and delete the rest of the git repository
5. Verify that the integration worked

We'll do this all in the `third_party` directory of `gpuopen`. In the depot, it's
located at `//depot/stg/gpuopen/gpuopen/third_party`. On my machine,
this is located at`%HOME%/p4/primary/drivers/gpuopen/gpuopen/third_party`.
YMMV.

Here we will use RapidJSON version 1.1.0, which is tagged in `git` as `v1.1.0`.
It was released Aug 25, 2016 and was the latest version when integrated in May
2018.
You can check the repository or the github page for the latest tagged release.

```bash
git clone --branch=v1.1.0 --depth=1 https://github.com/Tencent/rapidjson.git rapidjson.git
cp -v ./rapidjson.git/license.txt ./rapidjson/
cp -v ./rapidjson.git/readme*.md  ./rapidjson/
mv -v ./rapidjson.git/include     ./rapidjson/include/
rm -r ./rapidjson.git
```
Source code should then be able to use `rapidjson` by including under the
`rapidjson` folder.
e.g. to use `rapidjson::Writer<>`, use `#include <rapidjson/writer.h>`

## A note on `git`

We use a few arguments with git that aren't necessary, but speed things up.

- `--branch` specifies a branch or tag to clone directly. Without this, we'll
default to whatever the remote repository's `HEAD` points to. It's usually
`master`. It saves you a `cd $repo && git checkout`.
- `--depth=1` only downloads the data for a single commit. This downloads as
little of the history as we need - about 1 MiB. If you don't use it, the
download will take longer and as be as large as 23 MiB.

If you plan on keeping the git repo around for future use, you may not want to
include the `--depth` argument, as it will limit the project's history that you
can see. e.g. When using  `--depth=1`, you won't see any tags because there is
not git history available.
