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

import React, { useEffect, useState } from 'react';
import PropTypes from 'prop-types';
import { Table } from 'antd';
import { uuid } from 'cytoscape/src/util';
import CypherResultTab from '../../cytoscape/CypherResultTab';

const CypherResultTable = ({ data, ...props }) => {
  const [localColumns, setLocalColumns] = useState([]);
  const [localRows, setLocalRows] = useState([]);

  useEffect(() => {
    const randKeyName = `key_${uuid()}`;
    let hasKey = false;
    const columnsForFTable = [];
    data.columns.forEach((key) => {
      let isKey = false;
      if (key === 'key') {
        isKey = true;
        hasKey = true;
      }
      columnsForFTable.push({
        title: key,
        dataIndex: isKey ? randKeyName : key,
        key: isKey ? randKeyName : key,
        render: (text) => <>{JSON.stringify(text)}</>,
      });
    });
    setLocalColumns(columnsForFTable);

    if (props.filterTable?.length > 0) {
      const newItem = [];
      data.rows.forEach((item) => {
        props.filterTable.forEach((filter) => {
          if ((filter.property.label === item.r.label
            && item.r.properties[filter.property.property].includes(filter.keyword))
            || (filter.property.label === item.v.label
              && item.v.properties[filter.property.property].includes(filter.keyword))
            || (filter.property.label === item.v2.label
              && item.v2.properties[filter.property.property].includes(filter.keyword))) {
            newItem.push(item);
          }
        });
      });
      setLocalRows(newItem.map((item) => {
        const filteredItem = {
          ...item,
        };
        if (hasKey) {
          newItem[randKeyName] = newItem.key;
          delete newItem.key;
        }
        filteredItem.key = uuid();
        return filteredItem;
      }));
    } else {
      setLocalRows(data.rows.map((item) => {
        const newItem = {
          ...item,
        };
        if (hasKey) {
          newItem[randKeyName] = newItem.key;
          delete newItem.key;
        }
        newItem.key = uuid();
        return newItem;
      }));
    }
  }, [props.filterTable]);

  if (data.command && data.command.toUpperCase().match('(GRAPH|COPY|UPDATE).*')) {
    return (
      <div style={{ margin: '25px' }}>
        <span style={{ whiteSpace: 'pre-line' }}>
          <span>Successfully ran the query!</span>
        </span>
      </div>
    );
  } if (data.command && data.command.toUpperCase() === 'CREATE') {
    return <div style={{ margin: '25px' }}><span style={{ whiteSpace: 'pre-line' }}>{data.command.toUpperCase()}</span></div>;
  } if (data.command && data.command.toUpperCase() === 'ERROR') {
    return <div style={{ margin: '25px' }}><span style={{ whiteSpace: 'pre-line' }}>{data.message}</span></div>;
  } if (data.command === null) {
    return <div style={{ margin: '25px' }}><span style={{ whiteSpace: 'pre-line' }}>Query not entered!</span></div>;
  }

  const { refKey, setIsTable } = props;
  return (
    <div className="legend-area">
      <div className="contianer-frame-tab">
        <div style={{ width: '80%', color: '#C4C4C4' }}>
          <div className="d-flex nodeLegend">Node:</div>
          <div className="d-flex edgeLegend">Edge:</div>
        </div>
        <CypherResultTab refKey={refKey} setIsTable={setIsTable} currentTab="table" />
      </div>
      <Table columns={localColumns} dataSource={localRows} />
    </div>
  );
};

CypherResultTable.propTypes = {
  data: PropTypes.shape({
    message: PropTypes.string,
    command: PropTypes.string,
    rowCount: PropTypes.number,
    // eslint-disable-next-line react/forbid-prop-types
    columns: PropTypes.any,
    // eslint-disable-next-line react/forbid-prop-types
    rows: PropTypes.any,
    statusText: PropTypes.string,
  }).isRequired,
  refKey: PropTypes.string.isRequired,
  setIsTable: PropTypes.func.isRequired,
  // eslint-disable-next-line react/forbid-prop-types
  filterTable: PropTypes.any.isRequired,
};

export default CypherResultTable;
