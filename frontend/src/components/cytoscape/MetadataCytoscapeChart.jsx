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

import React, { useCallback, useEffect, useState } from 'react';
import PropTypes from 'prop-types';
import cytoscape from 'cytoscape';
import COSEBilkent from 'cytoscape-cose-bilkent';
import cola from 'cytoscape-cola';
import dagre from 'cytoscape-dagre';
import klay from 'cytoscape-klay';
import euler from 'cytoscape-euler';
import avsdf from 'cytoscape-avsdf';
import spread from 'cytoscape-spread';
import CytoscapeComponent from 'react-cytoscapejs';
import { seletableLayouts } from './CytoscapeLayouts';
import { stylesheet } from './CytoscapeStyleSheet';

import styles from '../frame/Frame.module.scss';

cytoscape.use(COSEBilkent);
cytoscape.use(cola);
cytoscape.use(dagre);
cytoscape.use(klay);
cytoscape.use(euler);
cytoscape.use(avsdf);
cytoscape.use(spread);

const MetadataCytoscapeChart = ({ elements }) => {
  const [cytoscapeObject, setCytoscapeObject] = useState(null);

  const cyCallback = useCallback((newCytoscapeObject) => {
    if (cytoscapeObject) return;
    newCytoscapeObject.on('resize', () => {
      try {
        newCytoscapeObject.center();
      } catch (e) {
        // todo check why is it occurs error
      }
    });
    setCytoscapeObject(newCytoscapeObject);
  },
  [cytoscapeObject]);

  useEffect(() => {
    if (cytoscapeObject && elements) {
      try {
        cytoscapeObject.add(CytoscapeComponent.normalizeElements(elements));
      } catch (e) {
        // todo: Metadata handling on server is wrong.
      }

      cytoscapeObject.minZoom(1e-1);
      cytoscapeObject.maxZoom(1.5);
      const selectedLayout = seletableLayouts.coseBilkent;
      selectedLayout.animate = true;
      selectedLayout.fit = true;
      cytoscapeObject.layout(selectedLayout).run();
      cytoscapeObject.maxZoom(5);
    }
  }, [elements]);

  return (
    <CytoscapeComponent
      elements={[]}
      stylesheet={stylesheet}
      cy={cyCallback}
      className={styles.MetaChart}
      wheelSensitivity={0.3}
    />
  );
};

MetadataCytoscapeChart.propTypes = {
  elements: PropTypes.shape({
    nodes: PropTypes.arrayOf(PropTypes.shape({
      // eslint-disable-next-line react/forbid-prop-types
      data: PropTypes.any,
    })),
    edges: PropTypes.arrayOf(PropTypes.shape({
      // eslint-disable-next-line react/forbid-prop-types
      data: PropTypes.any,
    })),
  }).isRequired,
};

export default MetadataCytoscapeChart;
