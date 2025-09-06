# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

from .age import Age, deleteGraph
from .models import *
from .builder import ResultHandler, DummyResultHandler, parseAgeValue, newResultHandler
from . import VERSION

def version():
    return VERSION.VERSION

async def connect(dsn: str = None, graph: str = None, load_from_plugins: bool = False, **kwargs):
    ag = Age()
    await ag.connect(dsn=dsn, graph=graph, load_from_plugins=load_from_plugins, **kwargs)
    return ag

# Dummy ResultHandler
rawPrinter = DummyResultHandler()

__name__="age"
