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

import React from 'react';
import PropTypes from 'prop-types';
import { Col, Row } from 'antd';

const CypherResultMeta = ({ database, query, data }) => (
  <>
    <Row>
      <Col span={6}>
        <b>Server Version</b>
      </Col>
      <Col span={18}>TBD</Col>
    </Row>
    <Row>
      <Col span={6}><b>Database URI</b></Col>
      <Col span={18}>
        {database.host}
        :
        {database.port}
      </Col>
    </Row>
    <Row>
      <Col span={6}><b>Executed Query</b></Col>
      <Col span={18}>{query}</Col>
    </Row>
    <Row>
      <Col span={6}><b>Data</b></Col>
      <Col span={18}><pre>{JSON.stringify(data, null, 2)}</pre></Col>
    </Row>
  </>
);

CypherResultMeta.propTypes = {
  database: PropTypes.shape({
    host: PropTypes.string.isRequired,
    port: PropTypes.string.isRequired,
  }).isRequired,
  query: PropTypes.string.isRequired,
  // eslint-disable-next-line react/forbid-prop-types
  data: PropTypes.any.isRequired,
};

export default CypherResultMeta;
