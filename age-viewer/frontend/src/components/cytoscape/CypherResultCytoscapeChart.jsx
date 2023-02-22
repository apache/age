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
import ReactDOMServer from 'react-dom/server';
import PropTypes from 'prop-types';
import cytoscape from 'cytoscape';
import COSEBilkent from 'cytoscape-cose-bilkent';
import cola from 'cytoscape-cola';
import dagre from 'cytoscape-dagre';
import klay from 'cytoscape-klay';
import euler from 'cytoscape-euler';
import avsdf from 'cytoscape-avsdf';
import spread from 'cytoscape-spread';
import { useDispatch } from 'react-redux';
import CytoscapeComponent from 'react-cytoscapejs';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';
import {
  faEyeSlash,
  faLockOpen,
  faProjectDiagram,
  faTrash,
} from '@fortawesome/free-solid-svg-icons';
import uuid from 'react-uuid';
import cxtmenu from '@lsvih/cytoscape-cxtmenu';
import { initLocation, seletableLayouts } from './CytoscapeLayouts';
import { stylesheet } from './CytoscapeStyleSheet';
import { generateCytoscapeElement } from '../../features/cypher/CypherUtil';
import IconFilter from '../../icons/IconFilter';
import IconSearchCancel from '../../icons/IconSearchCancel';
import styles from '../frame/Frame.module.scss';

cytoscape.use(COSEBilkent);
cytoscape.use(cola);
cytoscape.use(dagre);
cytoscape.use(klay);
cytoscape.use(euler);
cytoscape.use(avsdf);
cytoscape.use(spread);
cytoscape.use(cxtmenu);

const CypherResultCytoscapeCharts = ({
  elements,
  cytoscapeObject,
  setCytoscapeObject,
  cytoscapeLayout,
  maxDataOfGraph,
  onElementsMouseover,
  addLegendData,
  graph,
  onAddSubmit,
  onRemoveSubmit,
  openModal,
  addGraphHistory,
  addElementHistory,
}) => {
  const [cytoscapeMenu, setCytoscapeMenu] = useState(null);
  const [initialized, setInitialized] = useState(false);
  const dispatch = useDispatch();
  const addEventOnElements = (targetElements) => {
    targetElements.bind('mouseover', (e) => {
      onElementsMouseover({ type: 'elements', data: e.target.data() });
      e.target.addClass('highlight');
    });

    targetElements.bind('mouseout', (e) => {
      if (cytoscapeObject.elements(':selected').length === 0) {
        onElementsMouseover({
          type: 'background',
          data: {
            nodeCount: cytoscapeObject.nodes().size(),
            edgeCount: cytoscapeObject.edges().size(),
          },
        });
      } else {
        onElementsMouseover({
          type: 'elements',
          data: cytoscapeObject.elements(':selected')[0].data(),
        });
      }

      e.target.removeClass('highlight');
    });

    targetElements.bind('click', (e) => {
      const ele = e.target;
      if (ele.selected() && ele.isNode()) {
        if (cytoscapeObject.nodes(':selected').size() === 1) {
          ele.neighborhood().selectify().select().unselectify();
        } else {
          cytoscapeObject
            .nodes(':selected')
            .filter(`[id != "${ele.id()}"]`)
            .neighborhood()
            .selectify()
            .select()
            .unselectify();
        }
      } else {
        cytoscapeObject.elements(':selected').unselect().selectify();
      }
    });

    cytoscapeObject.bind('click', (e) => {
      if (e.target === cytoscapeObject) {
        cytoscapeObject.elements(':selected').unselect().selectify();
        onElementsMouseover({
          type: 'background',
          data: {
            nodeCount: cytoscapeObject.nodes().size(),
            edgeCount: cytoscapeObject.edges().size(),
          },
        });
      }
    });
  };

  const addElements = (centerId, d) => {
    const generatedData = generateCytoscapeElement(
      d.rows,
      maxDataOfGraph,
      true,
    );
    if (generatedData.elements.nodes.length === 0) {
      alert('No data to extend.');
      return;
    }

    cytoscapeObject.elements().lock();
    cytoscapeObject.add(generatedData.elements);

    const newlyAddedEdges = cytoscapeObject.edges('.new');
    const newlyAddedTargets = newlyAddedEdges.targets();
    const newlyAddedSources = newlyAddedEdges.sources();
    const rerenderTargets = newlyAddedEdges
      .union(newlyAddedTargets)
      .union(newlyAddedSources);

    const centerPosition = {
      ...cytoscapeObject.nodes().getElementById(centerId).position(),
    };
    cytoscapeObject.elements().unlock();
    rerenderTargets.layout(seletableLayouts.concentric).run();

    const centerMovedPosition = {
      ...cytoscapeObject.nodes().getElementById(centerId).position(),
    };
    const xGap = centerMovedPosition.x - centerPosition.x;
    const yGap = centerMovedPosition.y - centerPosition.y;
    rerenderTargets.forEach((ele) => {
      const pos = ele.position();
      ele.position({ x: pos.x - xGap, y: pos.y - yGap });
    });
    addEventOnElements(cytoscapeObject.elements('new'));

    addLegendData(generatedData.legend);
    rerenderTargets.removeClass('new');
  };

  useEffect(() => {
    if (cytoscapeMenu === null && cytoscapeObject !== null) {
      const cxtMenuConf = {
        menuRadius(ele) {
          return ele.cy().zoom() <= 1 ? 55 : 70;
        },
        selector: 'node',
        commands: [
          {
            content: ReactDOMServer.renderToString(
              <FontAwesomeIcon icon={faLockOpen} size="lg" />,
            ),
            select(ele) {
              ele.animate({ position: initLocation[ele.id()] });
            },
          },
          {
            content: ReactDOMServer.renderToString(
              <FontAwesomeIcon icon={faProjectDiagram} size="lg" />,
            ),
            select(ele) {
              const elAnimate = ele.animation({
                style: {
                  'border-color': 'green',
                  'border-width': '11px',
                },
                duration: 1000,
              });
              elAnimate.play();
              const animateTimer = setInterval(() => {
                if (elAnimate.complete()) {
                  elAnimate.reverse().play();
                }
              }, 1000);

              fetch('/api/v1/cypher', {
                method: 'POST',
                headers: {
                  Accept: 'application/json',
                  'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                  cmd: `SELECT * FROM cypher('${graph}', $$ MATCH (S)-[R]-(T) WHERE id(S) = ${ele.id()} RETURN S, R, T $$) as (S agtype, R agtype, T agtype);`,
                }),
              })
                .then((res) => res.json())
                .then((data) => {
                  elAnimate.rewind().stop();
                  clearInterval(animateTimer);
                  addElements(ele.id(), data);
                });
            },
          },
          {
            content: ReactDOMServer.renderToString(
              <FontAwesomeIcon icon={faEyeSlash} size="lg" />,
            ),
            select(ele) {
              ele.remove();
            },
          },
          {
            content: ReactDOMServer.renderToString(
              <FontAwesomeIcon icon={faTrash} size="lg" />,
            ),
            select(ele) {
              dispatch(openModal());
              dispatch(addGraphHistory(graph));
              dispatch(addElementHistory(ele.id()));
            },
          },
          {
            content: ReactDOMServer.renderToString(<IconFilter size="lg" />),
            select(ele) {
              const newFilterObject = {
                key: uuid(),
                keyword: ele.data().properties[ele.data().caption],
                property: {
                  label: ele.data().label,
                  property: ele.data().caption,
                },
              };
              onAddSubmit(newFilterObject);
            },
          },
          {
            content: ReactDOMServer.renderToString(
              <IconSearchCancel size="lg" />,
            ),
            select(ele) {
              const keywordObject = {
                keyword: ele.data().properties[ele.data().caption],
              };
              onRemoveSubmit(keywordObject);
            },
          },
        ],
        fillColor: 'rgba(210, 213, 218, 1)',
        activeFillColor: 'rgba(166, 166, 166, 1)',
        activePadding: 0,
        indicatorSize: 0,
        separatorWidth: 4,
        spotlightPadding: 3,
        minSpotlightRadius: 11,
        maxSpotlightRadius: 99,
        openMenuEvents: 'cxttap',
        itemColor: '#2A2C34',
        itemTextShadowColor: 'transparent',
        zIndex: 9999,
        atMouse: false,
      };
      setCytoscapeMenu(cytoscapeObject.cxtmenu(cxtMenuConf));
    }
  }, [cytoscapeObject, cytoscapeMenu]);

  useEffect(() => {
    if (cytoscapeLayout && cytoscapeObject) {
      const selectedLayout = seletableLayouts[cytoscapeLayout];
      selectedLayout.animate = true;
      selectedLayout.fit = true;

      cytoscapeObject.minZoom(1e-1);
      cytoscapeObject.maxZoom(1.5);
      cytoscapeObject.layout(selectedLayout).run();
      cytoscapeObject.maxZoom(5);
      if (!initialized) {
        addEventOnElements(cytoscapeObject.elements());
        setInitialized(true);
      }
    }
  }, [cytoscapeObject, cytoscapeLayout]);

  const cyCallback = useCallback(
    (newCytoscapeObject) => {
      if (cytoscapeObject) return;
      setCytoscapeObject(newCytoscapeObject);
    },
    [cytoscapeObject],
  );

  return (
    <CytoscapeComponent
      elements={CytoscapeComponent.normalizeElements(elements)}
      stylesheet={stylesheet}
      cy={cyCallback}
      className={styles.NormalChart}
      wheelSensitivity={0.3}
    />
  );
};

CypherResultCytoscapeCharts.defaultProps = {
  cytoscapeObject: null,
};

CypherResultCytoscapeCharts.propTypes = {
  elements: PropTypes.shape({
    nodes: PropTypes.arrayOf(
      PropTypes.shape({
        // eslint-disable-next-line react/forbid-prop-types
        data: PropTypes.any,
      }),
    ),
    edges: PropTypes.arrayOf(
      PropTypes.shape({
        // eslint-disable-next-line react/forbid-prop-types
        data: PropTypes.any,
      }),
    ),
  }).isRequired,
  // eslint-disable-next-line react/forbid-prop-types
  cytoscapeObject: PropTypes.any,
  setCytoscapeObject: PropTypes.func.isRequired,
  cytoscapeLayout: PropTypes.string.isRequired,
  maxDataOfGraph: PropTypes.number.isRequired,
  onElementsMouseover: PropTypes.func.isRequired,
  addLegendData: PropTypes.func.isRequired,
  graph: PropTypes.string.isRequired,
  onAddSubmit: PropTypes.func.isRequired,
  onRemoveSubmit: PropTypes.func.isRequired,
  openModal: PropTypes.func.isRequired,
  addGraphHistory: PropTypes.func.isRequired,
  addElementHistory: PropTypes.func.isRequired,
};

export default CypherResultCytoscapeCharts;
