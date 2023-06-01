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
import Sidebar from '../presentations/Sidebar';

const mapStateToProps = (state) => ({
  activeMenuName: state.navigator.activeMenu,
  isActive: state.navigator.isActive,
});

const mapDispatchToProps = {};

export default connect(mapStateToProps, mapDispatchToProps)(Sidebar);

/*
import React, {Component} from 'react'
import store from '../../../app/store'
class SidebarContainer extends Component {
    constructor(props) {
        super(props);
        this.state = {navigator : store.getState().navigator}
    }

    componentDidMount() {
        store.subscribe(function() {
            this.setState({navigator : store.getState().navigator});
        }.bind(this));
    }

    changeTheme = (e) => {
        console.log(e);
        const selectedTheme = e.target.value
        store.dispatch({type: 'CHANGE_THEME', theme : selectedTheme})
    }

    render() {
        const activeMenu  = this.state.navigator.activeMenu
        return (
            <Sidebar activeMenuName={activeMenu} onThemeChange={this.changeTheme} />
        );
    }

}

export default SidebarContainer
*/
