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
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';

const NavigatorItem = ({ itemInfo, activeMenuName, onClick }) => {
  const [menuName, fwCode] = itemInfo;
  return (
    <li className="nav-item">
      <a
        id={`side-${menuName}-tab`}
        className={`nav-link${activeMenuName === menuName ? ' active show ' : ''}`}
        data-classname="fixed-left"
        data-toggle="pill"
        href="/#"
        role="tab"
        aria-controls={`side-${menuName}`}
        aria-selected="true"
        onClick={() => onClick(menuName)}
      >
        <FontAwesomeIcon icon={fwCode} />
      </a>
    </li>
  );
};

NavigatorItem.propTypes = {
  itemInfo: PropTypes.arrayOf(PropTypes.any).isRequired,
  activeMenuName: PropTypes.string.isRequired,
  onClick: PropTypes.func.isRequired,
};

export default NavigatorItem;
