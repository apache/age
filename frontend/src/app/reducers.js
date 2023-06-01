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

import { combineReducers } from 'redux';
import DatabaseReducer from '../features/database/DatabaseSlice';
import MetadataReducer from '../features/database/MetadataSlice';
import FrameReducer from '../features/frame/FrameSlice';
import MenuReducer from '../features/menu/MenuSlice';
import SettingReducer from '../features/setting/SettingSlice';
import CypherReducer from '../features/cypher/CypherSlice';
import AlertReducer from '../features/alert/AlertSlice';
import EditorSlice from '../features/editor/EditorSlice';
import ModalSlice from '../features/modal/ModalSlice';
import LayoutSlice from '../features/layout/LayoutSlice';

const rootReducer = combineReducers({
  navigator: MenuReducer,
  setting: SettingReducer,
  database: DatabaseReducer,
  metadata: MetadataReducer,
  frames: FrameReducer,
  cypher: CypherReducer,
  alerts: AlertReducer,
  editor: EditorSlice,
  modal: ModalSlice,
  layout: LayoutSlice,
});

export default rootReducer;
