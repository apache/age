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

from setuptools import setup, find_packages
from age import VERSION

with open("README.md", "r", encoding='utf8') as fh:
    long_description = fh.read()

setup(
    name             = 'apache-age-asyncpg',
    version          = '0.1.0',
    description      = 'An async Python driver for Apache AGE, based on asyncpg.',
    long_description=long_description,
    long_description_content_type="text/markdown",
    author           = 'Apache AGE',
    author_email     = 'dev@age.apache.org',
    url              = 'https://github.com/apache/age/tree/master/drivers/python-asyncpg',
    download_url     = 'https://github.com/apache/age/releases' ,
    license          = 'Apache License 2.0',
    install_requires = [ 'asyncpg', 'antlr4-python3-runtime==4.11.1', 'networkx' ],
    packages         = find_packages(),
    keywords         = ['Graph Database', 'Apache AGE', 'PostgreSQL', 'asyncio', 'asyncpg'],
    python_requires  = '>=3.9',
    classifiers      = [
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: Apache Software License',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Operating System :: OS Independent',
        'Topic :: Database',
        'Framework :: AsyncIO',
    ],
    extras_require = {
        'testing': ["pytest", "pytest-asyncio"],
    }
)
