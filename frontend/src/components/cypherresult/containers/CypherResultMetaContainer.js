/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

import { connect } from 'react-redux';
import CypherResultMeta from '../presentations/CypherResultMeta';

const mapStateToProps = (state, ownProps) => {
  const { refKey } = ownProps;

  let database = {};
  let query = '';
  let data = {};
  if (state.cypher.queryResult[refKey]) {
    database = state.database;
    query = state.cypher.queryResult[refKey].query;
    data = {
      columns: state.cypher.queryResult[refKey].columns,
      command: state.cypher.queryResult[refKey].command,
      rowCount: state.cypher.queryResult[refKey].rowCount,
      rows: state.cypher.queryResult[refKey].rows,
      message: state.cypher.queryResult[refKey].message,
    };
  }

  return {
    database,
    query,
    data,
  };
};

const mapDispatchToProps = {};

export default connect(mapStateToProps, mapDispatchToProps)(CypherResultMeta);
