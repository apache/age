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

from .age import Age
from .models import Model, Property, Relationship
from .builder import ResultHandler, parseAgeValue, newResultHandler
from . import VERSION


def version() -> str:
    """
    Returns the version of the Age library.
    """
    return VERSION.VERSION


def connect(dsn=None, graph=None, connection_factory=None, cursor_factory=None, **kwargs) -> Age:
    """
    Establishes a connection to an Age graph database and returns an Age object.
    """
    ag = Age()
    ag.connect(dsn=dsn, graph=graph, connection_factory=connection_factory,
               cursor_factory=cursor_factory, **kwargs)
    return ag


# Result Handlers
class RawPrinter(ResultHandler):
    """
    Result handler that prints raw query results to the console.
    """

    def __init__(self):
        super().__init__()

    def handleResult(self, result):
        print(result)


__name__ = "age"
