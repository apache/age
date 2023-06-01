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
import { Button, message, Upload } from 'antd';
import { useDispatch } from 'react-redux';
import Frame from '../frame/Frame';
import { getMetaData } from '../../features/database/MetadataSlice';

const CSV = ({
  reqString, refKey,
}) => {
  const dispatch = useDispatch();

  const props = {
    name: 'file',
    action: '/api/v1/feature/uploadCSV',
    headers: {
      authorization: 'authorization-text',
    },
    onChange(info) {
      if (info.file.status === 'done') {
        dispatch(getMetaData());
        message.success(`${info.file.name} file uploaded successfully`);
      } else if (info.file.status === 'error') {
        message.error(`${info.file.name} file upload failed.`);
      }
    },
  };

  return (
    <Frame
      reqString={reqString}
      refKey={refKey}
    >
      {/* eslint-disable-next-line react/jsx-props-no-spreading */}
      <Upload {...props}>
        <Button>Click to Upload</Button>
      </Upload>
    </Frame>
  );
};

CSV.propTypes = {
  reqString: PropTypes.string.isRequired,
  refKey: PropTypes.string.isRequired,

};

export default CSV;
