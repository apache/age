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



def SQL_LoadAge():
    return """LOAD 'age';"""

def SQL_Set_Searchpath_to_ag_catalog():
    return """SET search_path = ag_catalog, '$user', public;"""

def SQL_Create_graph(graphName : str):
    return f"""SELECT create_graph('{graphName}');"""

def SQL_Delete_graph(graphName : str):
    return f"""SELECT drop_graph('{graphName}', true);"""

def SQL_CountGraph(graphName : str):
    return f"""SELECT count(*) FROM ag_graph WHERE name='{graphName}';"""