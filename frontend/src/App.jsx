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
import 'bootstrap/dist/css/bootstrap.min.css';
import 'antd/dist/antd.css';
import './static/style.css';
import './static/navbar-fixed-left.css';
import { BrowserRouter as Router, Route, Routes } from "react-router-dom";
import MainPage from './pages/Main/MainPage';
import First from './pages/Main/First';

function App() {
  return (
    <Router>
      <Routes>
        <Route exact path="/" element={<First/>} />
        <Route exact path="/second" element={<MainPage/>} />
      </Routes>
    </Router>
  );
}
// const App = () => (
//   <React.StrictMode>
//     <First />
//   </React.StrictMode>
// );

export default App;
