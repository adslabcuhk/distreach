source ../scripts/common.sh
DIRNAME="distfarreach"

echo "stop servers"
ssh ${USER}@dl16 "cd projects/NetBuffer/${DIRNAME}; bash localscripts/stop_server.sh >/dev/null 2>&1"
ssh ${USER}@dl13 "cd projects/NetBuffer/${DIRNAME}; bash localscripts/stop_server.sh >/dev/null 2>&1"

echo "stop controller"
ssh ${USER}@dl16 "cd projects/NetBuffer/${DIRNAME}; bash localscripts/stop_controller.sh >/dev/null 2>&1"

echo "stop reflectors"
ssh ${USER}@dl16 "cd projects/NetBuffer/${DIRNAME}; bash localscripts/stop_reflector.sh >/dev/null 2>&1"
sudo bash localscripts/stop_reflector.sh

echo "kill servers"
ssh ${USER}@dl16 "cd projects/NetBuffer/${DIRNAME}; bash localscripts/kill_server.sh >/dev/null 2>&1"
ssh ${USER}@dl13 "cd projects/NetBuffer/${DIRNAME}; bash localscripts/kill_server.sh >/dev/null 2>&1"

echo "kill controller"
ssh ${USER}@dl16 "cd projects/NetBuffer/${DIRNAME}; bash localscripts/kill_controller.sh >/dev/null 2>&1"
