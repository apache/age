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
import CypherResultTable from '../presentations/CypherResultTable';

const mapStateToProps = (state, ownProps) => {
  const { refKey } = ownProps;
  const generateTableData = (data) => {
    let columns = [];
    let rows = [];
    let command;
    let rowCount = null;
    let message = '';

    if (data && data.command !== 'ERROR' && data.command !== null) {
      columns = data.columns;
      rows = data.rows.slice(0, (state.setting.maxDataOfTable === 0
        ? data.rows.length : state.setting.maxDataOfTable));
      command = data.command;
      rowCount = data.rowCount;
    } else {
      command = data.command;
      message = data.message;
    }
    return {
      command, rowCount, columns, rows, message,
    };
  };
  return {
    data: generateTableData(state.cypher.queryResult[refKey]),
  };
};

const mapDispatchToProps = { };

export default connect(mapStateToProps, mapDispatchToProps)(CypherResultTable);
