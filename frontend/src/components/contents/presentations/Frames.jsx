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
import { useDispatch } from 'react-redux';
import PropTypes from 'prop-types';
import Contents from '../../frame/containers/ContentsFrameContainer';
import ServerStatus from '../../frame/containers/ServerStatusContainer';
import ServerConnect from '../../frame/containers/ServerConnectContainer';
import ServerDisconnect from '../../frame/containers/ServerDisconnectContainer';
import CypherGraphResult from '../../frame/containers/CypherGraphResultContainers';
import CypherResult from '../../frame/containers/CypherResultContainers';
import CSV from '../../csv';
import { setting } from '../../../conf/config';

const Frames = ({
  database,
  frameList,
  addFrame,
  queryResult,
  maxNumOfFrames,
}) => {
  const dispatch = useDispatch();
  const [frames, setFrames] = useState(null);

  useEffect(() => {
    if (database.status === 'connected' && frameList.length === 0) {
      if (!setting.connectionStatusSkip) {
        dispatch(() => addFrame(':server status', 'ServerStatus'));
      }
    }

    if (database.status === 'disconnected') {
      const serverConnectFrames = frameList.filter((frame) => (frame.frameName.toUpperCase() === 'SERVERCONNECT'));
      if (!setting.closeWhenDisconnect) {
        dispatch(() => addFrame(':server connect', 'ServerConnect'));
      } else if (serverConnectFrames.length === 0) {
        window.close();
      }
    }
  }, [database.status]);

  useEffect(() => {
    setFrames(frameList.map((frame, index) => {
      if (index > maxNumOfFrames && maxNumOfFrames !== 0) {
        return '';
      }

      if (frame.frameName === 'Contents') {
        return (
          <Contents
            key={frame.frameProps.key}
            refKey={frame.frameProps.key}
            reqString={frame.frameProps.reqString}
            playTarget={frame.frameProps.playTarget}
            isPinned={frame.isPinned}
          />
        );
      }
      if (frame.frameName === 'CSV') {
        return (
          <CSV
            key={frame.frameProps.key}
            refKey={frame.frameProps.key}
            reqString={frame.frameProps.reqString}
            isPinned={frame.isPinned}
          />
        );
      }
      if (frame.frameName === 'ServerStatus') {
        return (
          <ServerStatus
            key={frame.frameProps.key}
            refKey={frame.frameProps.key}
            reqString={frame.frameProps.reqString}
            isPinned={frame.isPinned}
          />
        );
      }
      if (frame.frameName === 'ServerConnect') {
        return (
          <ServerConnect
            key={frame.frameProps.key}
            refKey={frame.frameProps.key}
            reqString={frame.frameProps.reqString}
            isPinned={frame.isPinned}
          />
        );
      }
      if (frame.frameName === 'ServerDisconnect') {
        return (
          <ServerDisconnect
            key={frame.frameProps.key}
            refKey={frame.frameProps.key}
            reqString={frame.frameProps.reqString}
            isPinned={frame.isPinned}
          />
        );
      }
      if (frame.frameName === 'CypherResultFrame') {
        if (queryResult[frame.frameProps.key]?.complete && (queryResult[frame.frameProps.key].command !== null ? queryResult[frame.frameProps.key].command.toUpperCase() : 'NULL')
          .match('(ERROR|GRAPH|CREATE|UPDATE|COPY|NULL).*')) {
          return (
            <CypherResult
              key={frame.frameProps.key}
              refKey={frame.frameProps.key}
              reqString={frame.frameProps.reqString}
              isPinned={frame.isPinned}
            />
          );
        }
        return (
          <CypherGraphResult
            key={frame.frameProps.key}
            refKey={frame.frameProps.key}
            reqString={frame.frameProps.reqString}
            isPinned={frame.isPinned}
          />
        );
      }
      return '';
    }));
  }, [frameList, queryResult]);

  return (
    <div className="container-fluid frame-area pt-3">
      {frames}
    </div>
  );
};

Frames.defaultProps = {
  queryResult: {},
};

Frames.propTypes = {
  database: PropTypes.shape({
    status: PropTypes.string.isRequired,
    host: PropTypes.string.isRequired,
  }).isRequired,
  frameList: PropTypes.arrayOf(
    PropTypes.shape({
      frameName: PropTypes.string.isRequired,
      frameProps: PropTypes.shape({
        reqString: PropTypes.string.isRequired,
        key: PropTypes.string.isRequired,
        playTarget: PropTypes.string,
      }).isRequired,
      isPinned: PropTypes.bool.isRequired,
    }),
  ).isRequired,
  addFrame: PropTypes.func.isRequired,
  // todo: need to refactoring on management Cypher Results
  // eslint-disable-next-line react/forbid-prop-types
  queryResult: PropTypes.any,
  maxNumOfFrames: PropTypes.number.isRequired,
};

export default Frames;
