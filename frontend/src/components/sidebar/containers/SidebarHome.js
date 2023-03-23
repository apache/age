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
import SidebarHome from '../presentations/SidebarHome';
import { setCommand } from '../../../features/editor/EditorSlice';
import { addFrame, trimFrame } from '../../../features/frame/FrameSlice';
import { changeGraph } from '../../../features/database/DatabaseSlice';
import { getMetaData, changeCurrentGraph } from '../../../features/database/MetadataSlice';

const mapStateToProps = (state) => {
  const currentGraphData = state.metadata.graphs[state.metadata.currentGraph] || '';
  return {
    currentGraph: state.metadata.currentGraph,
    graphs: Object.entries(state.metadata.graphs).map(([k, v]) => [k, v.id]),
    edges: currentGraphData.Edges,
    nodes: currentGraphData.Nodes,
    propertyKeys: currentGraphData.propertyKeys,
    dbname: state.metadata.dbname,
    status: state.metadata.status,
    role: currentGraphData.role,
    command: state.editor.command,
    isLabel: state.layout.isLabel,
  };
};

const mapDispatchToProps = {
  setCommand, addFrame, trimFrame, getMetaData, changeCurrentGraph, changeGraph,
};

export default connect(mapStateToProps, mapDispatchToProps)(SidebarHome);
