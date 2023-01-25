CREATE TEMP TABLE tmp (
  node_id text,
  hostname text,
  port text,
  status text,
  pg_status text,
  lb_weight text,
  role text,
  pg_role text,
  select_cnt text,
  load_balance_node text,
  replication_delay text,
  replication_state text,
  replication_sync_state text,
  last_status_change text,
  mode text);

INSERT INTO tmp VALUES
('0','localhost','11002','down','down','0.500000','standby','unknown','0','false','0','','','XXXX-XX-XX XX:XX:XX','s'),
('1','localhost','11003','up','up','0.500000','primary','unknown','0','false','0','','','XXXX-XX-XX XX:XX:XX','s'),
('0','localhost','11002','down','down','0.500000','replica','replica','0','false','0','','','XXXX-XX-XX XX:XX:XX','r'),
('1','localhost','11003','up','up','0.500000','main','main','0','false','0','','','XXXX-XX-XX XX:XX:XX','r');

SELECT node_id,hostname,port,status,pg_status,lb_weight,role,pg_role,select_cnt,load_balance_node,replication_delay,replication_state, replication_sync_state, last_status_change
FROM tmp
WHERE mode = :mode
