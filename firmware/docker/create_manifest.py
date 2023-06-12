#!/usr/bin/env python

import os
import json
import sys

if __name__ == "__main__":

    env = os.environ

    manifest = {
        "type": "esp32-fota-http",
        "version": env.get("CVERSION", 0),
        "host": env.get("SRVNAME", "espcamsrv1"),
        "port": env.get("SRVPORT", 8085),
        "bin": "/fota/firmware",
    }

    manifest["version"] = int(manifest["version"])
    manifest["port"] = int(manifest["port"])

    out_file = sys.argv[1]
    fd = open(out_file, "w")
    fd.write(json.dumps(manifest))
    fd.close()

