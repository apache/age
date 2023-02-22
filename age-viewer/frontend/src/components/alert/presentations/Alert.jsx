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

import React, { useEffect } from 'react';
import { useDispatch } from 'react-redux';
import PropTypes from 'prop-types';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import { faPlayCircle } from '@fortawesome/free-regular-svg-icons';
import { Alert } from 'antd';

const SingleAlert = ({
  alertKey,
  alertName,
  errorMessage,
  setCommand,
  removeAlert,
}) => {
  const dispatch = useDispatch();

  const setAlertConnect = (e, command) => {
    e.preventDefault();
    dispatch(() => {
      setCommand(command);
    });
  };

  const clearAlert = () => {
    dispatch(() => {
      removeAlert(alertKey);
    });
  };

  useEffect(() => {
    const timer = setTimeout(() => {
      clearAlert();
    }, 10000);
    return () => clearTimeout(timer);
  }, []);

  if (alertName === 'NoticeServerDisconnected') {
    return (
      <Alert
        variant="warning"
        afterClose={() => clearAlert()}
        showIcon
        closable
        message="Database Disconnected"
        description={(
          <p>
            Database is Disconnected. You may use
            {' '}
            <button type="button" className="badge badge-light" onClick={(e) => setAlertConnect(e, ':server connect')}>

              <FontAwesomeIcon
                icon={faPlayCircle}
                size="lg"
              />
              :server connect
            </button>
            {' '}
            to
            establish connection. There&apos;s a graph waiting for you.
          </p>
        )}
      />
    );
  }
  if (alertName === 'NoticeServerConnected') {
    return (
      <Alert
        type="success"
        afterClose={() => clearAlert()}
        showIcon
        closable
        message="Database Connected"
        description={(
          <p>
            Successfully database is connected. You may use
            {' '}
            <a href="/#" className="badge badge-light" onClick={(e) => setAlertConnect(e, ':server status')}>
              <FontAwesomeIcon
                icon={faPlayCircle}
                size="lg"
              />
              :server status
            </a>
            {' '}
            to
            confirm connected database information.
          </p>
        )}
      />
    );
  }
  if (alertName === 'ErrorServerConnectFail') {
    return (
      <Alert
        type="error"
        afterClose={() => clearAlert()}
        showIcon
        closable
        message="Database Connection Failed"
        description={(
          <>
            <p>
              Failed to connect to the database. Are you sure the database is running on the server?
            </p>
            {errorMessage}
          </>
        )}
      />
    );
  }
  if (alertName === 'ErrorNoDatabaseConnected') {
    return (
      <Alert
        type="error"
        showIcon
        closable
        afterClose={() => clearAlert()}
        message="No Database Connected"
        description={
        (
          <>
            <p>
              You haven&apos;t set database connection. You may use
              {' '}
              <a href="/#" className="badge badge-light" onClick={(e) => setAlertConnect(e, ':server connect')}>
                <FontAwesomeIcon
                  icon={faPlayCircle}
                  size="lg"
                />
                :server connect
              </a>
              {' '}
              to
              establish connection. There&apos;s a graph waiting for you.
            </p>
            {errorMessage}
          </>
        )
      }
      />
    );
  }
  if (alertName === 'ErrorMetaFail') {
    return (
      <Alert
        type="error"
        afterClose={() => clearAlert()}
        message="Metadata Load Error"
        showIcon
        closable
        description={(
          <p>
            Unexpectedly error occurred while getting metadata.
          </p>
        )}
      />
    );
  }
  if (alertName === 'ErrorCypherQuery') {
    return (
      <Alert
        type="error"
        afterClose={() => clearAlert()}
        showIcon
        closable
        message="Query Error"
        description={(
          <p>
            Your query was not executed properly. Refer the below error message.
          </p>
        )}
      />
    );
  }
  if (alertName === 'ErrorPlayLoadFail') {
    return (
      <Alert
        type="error"
        afterClose={() => clearAlert()}
        showIcon
        closable
        message="Failed to Load Play Target"
        description={(
          <p>
            &apos;
            {errorMessage}
            &apos; does not exists.
          </p>
        )}
      />
    );
  }
  if (alertName === 'NoticeAlreadyConnected') {
    return (
      <Alert
        type="info"
        afterClose={() => clearAlert()}
        showIcon
        closable
        message="Already Connected to Database"
        description={(
          <p>
            You are currently connected to a database.
            If you want to access to another database, you may execute
            <a
              href="/#"
              className="badge badge-light"
              onClick={(e) => setAlertConnect(e, ':server disconnect')}
            >
              <FontAwesomeIcon
                icon={faPlayCircle}
                size="lg"
              />
              :server disconnect
            </a>
            {' '}
            to disconnect from current database first.
          </p>
        )}
      />
    );
  }
  if (alertName === 'CreateGraphSuccess') {
    return (
      <Alert type="success" message="Graph Created" description="Successfully created new graph" />
    );
  }
  return (<></>);
};
SingleAlert.propTypes = {
  alertKey: PropTypes.string.isRequired,
  alertName: PropTypes.string.isRequired,
  errorMessage: PropTypes.string.isRequired,
  setCommand: PropTypes.func.isRequired,
  removeAlert: PropTypes.func.isRequired,
};

export default SingleAlert;
