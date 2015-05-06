/**
 * OnVIF plugin
 */
function OnVIFPlugin(devices, agocontrol)
{
    //members
    var self = this;
    self.agocontrol = agocontrol;
    self.controllerUuid = null;
    self.cameras = ko.observableArray([]);
    self.cameraProfiles = ko.observableArray([]);
    self.cameraIp = ko.observable(null);
    self.cameraPort = ko.observable(null);
    self.cameraLogin = ko.observable(null);
    self.cameraPassword = ko.observable('');
    self.cameraInternalid = ko.observable(null);
    self.cameraUri = ko.observable(null);
    self.selectedCamera = ko.observable(null);
    self.generalData = ko.observableArray(null);
    self.configData = ko.observableArray(null);
    self.onvifService = ko.observable(null);
    self.onvifOperation = ko.observable(null);
    self.onvifParameters = ko.observable(JSON.stringify({'key': 'value'}, null, 4));
    self.onvifResponse = ko.observable('');
    self.onvifServices = ko.observableArray(['devicemgmt', 'deviceio', 'event', 'analytics', 'analyticsdevice', 'display', 'imaging', 'media', 'ptz', 'receiver', 'remotediscovery', 'recording', 'replay', 'search']);
    self.configSetOperation = ko.observable(null);
    self.configSetService = ko.observable(null);
    self.yesNoOptions = ko.observableArray([{caption:'No', value:0}, {caption:'Yes', value:1}]);
    self.motionEnable = ko.observable(null);
    self.motionProfile = ko.observable(null);
    self.motionSensitivity = ko.observable(10);
    self.motionDeviation = ko.observable(20);
    self.motionOnDuration = ko.observable(300);
    self.recordingsDir = ko.observable('/var/opt/agocontrol/recordings');
    self.recordingsDelays = ko.observableArray([{caption:'1 day', value:1440}, {caption:'1 week', value:10080}, {caption:'2 weeks', value:20160}, {caption:'1 month', value:44640}, {caption:'2 months', value:89280}]);
    self.recordingsDelay = ko.observable(10080); //1 week
    self.recordingTypes = ko.observableArray([{value:0, caption:'Disabled'}, {value:1, caption:'Record motion'}, {value:2, caption:'Record timelapse'}, {value:3, caption:'Record all'}]);
    self.recordingType = ko.observable(0);
    self.recordingProfile = ko.observable(null);
    self.recordingDuration = ko.observable(0);
    self.recordingContourTypes = ko.observableArray([{value:0, caption:'Disabled'}, {value:1, caption:'Single box (all areas merged)'}, {value:2, caption:'All detected areas'}]);
    self.recordingContourType = ko.observable(null);
    self.recordings = ko.observableArray([]);

    self.camerasGrid = new ko.agoGrid.viewModel({
        data: self.cameras,
        columns: [
            {headerText:'Camera ip', rowText:'ip'},
            {headerText:'Selected profile', rowText:'uri_desc'},
            {headerText:'Actions', rowText:''}
        ],
        rowTemplate: 'camerasRowTemplate'
    });

    self.recordingsGrid = new ko.agoGrid.viewModel({
        data: self.recordings,
        columns: [
            {headerText:'Filename', rowText:'filename'},
            {headerText:'Type', rowText:'type'},
            {headerText:'Datetime', rowText:'timestamp'},
            {headerText:'Size', rowText:'size'},
            {headerText:'Actions', rowText:''}
        ],
        rowTemplate: 'recordingsRowTemplate'
    });
    
    //get agoalert uuid
    if( devices!==undefined )
    {
        for( var i=0; i<devices.length; i++ )
        {
            if( devices[i].devicetype=='onvifcontroller' )
            {
                self.controllerUuid = devices[i].uuid;
            }
        }
    }

    //get config
    self.getConfig = function()
    {
        content = {};
        content.uuid = self.controllerUuid;
        content.command = 'getconfig';
        self.agocontrol.sendCommand(content)
        .then(function(resp) {
            if( resp.data.cameras && resp.data.general )
            {
                //set cameras
                var cameras = [];
                for( var internalid in resp.data.cameras )
                {
                    var camera = resp.data.cameras[internalid];
                    camera.internalid = internalid;
                    cameras.push(camera);
                }
                self.cameras(cameras);

                //set recordings
                self.recordings(resp.data.recordings);

                //set recordings config
                self.recordingsDir(resp.data.general.record_dir);
                self.recordingsDelay(resp.data.general.record_delay);
            }
            else
            {
                notif.error('Invalid configuration');
            }
        });
    };

    //get all cameras
    self.getCameras = function()
    {
        content = {};
        content.uuid = self.controllerUuid;
        content.command = 'getcameras';
        self.agocontrol.sendCommand(content)
        .then(function(resp) {
            cameras = [];
            for( var internalid in resp.data )
            {
                var camera = resp.data[internalid];
                camera.internalid = internalid;
                cameras.push(camera);
            }
            self.cameras(cameras);
        })
    };

    //get all recordings
    self.getRecordings = function()
    {
        content = {};
        content.uuid = self.controllerUuid;
        content.command = 'getrecordings';
        self.agocontrol.sendCommand(content)
        .then(function(resp) {
            self.recordings(resp.data);
        })
    };

    //add camera
    self.addCamera = function()
    {
        //don't check password, can be empty by default
        if( self.cameraIp() && self.cameraPort() && self.cameraLogin() && self.motionProfile() )
        {
            self.agocontrol.block('#agoGrid');
            self.agocontrol.block('#configTab');

            //fix password if null
            if( self.cameraPassword()===null )
            {
                self.cameraPassword('');
            }

            content = {};
            content.uuid = self.controllerUuid;
            content.command = 'addcamera';
            content.ip = self.cameraIp();
            content.port = self.cameraPort();
            content.login = self.cameraLogin();
            content.password = self.cameraPassword();
            content.uri_token = self.motionProfile().token;
            content.uri_desc = self.motionProfile().desc;
            self.agocontrol.sendCommand(content, null, 20)
            .then(function(resp) {
                //reset form
                self.cameraIp(null);
                self.cameraPort(null);
                self.cameraLogin(null);
                self.cameraPassword('');
                self.motionProfile(null);
                self.cameraProfiles([]);

                //refresh cameras list
                self.getCameras();
            })
            .finally(function() {
                self.agocontrol.unblock('#agoGrid');
                self.agocontrol.unblock('#configTab');
            });
        }
        else
        {
            notif.info('Please fill all parameters');
        }
    };

    //get camera profiles (with internalid)
    self.getCameraProfiles = function()
    {
        if( self.selectedCamera() && self.selectedCamera().internalid )
        {
            self.agocontrol.block('#cameraDetails');

            content = {};
            content.uuid = self.controllerUuid;
            content.command = 'getprofiles';
            content.internalid = self.selectedCamera().internalid;
            //timeout increased because camera could take time to reply
            self.agocontrol.sendCommand(content, null, 20)
            .then(function(resp) {
                var profiles = [];
                for( i=0; i<resp.data.length; i++ )
                {
                    var profile = {};
                    profile.desc =  resp.data[i].encoding + ' ';
                    profile.desc += resp.data[i].resolution.width + 'x' + resp.data[i].resolution.height;
                    profile.desc += ' @' + resp.data[i].fps + 'fps';
                    profile.token = resp.data[i].uri_token;
                    profiles.push(profile);
                }
                self.cameraProfiles(profiles);

                //select current motion profile
                for( var i=0; i<self.cameraProfiles().length; i++ )
                {
                    if( self.cameraProfiles()[i].token==self.selectedCamera().motion_uri_token )
                    {
                        self.motionProfile(self.cameraProfiles()[i]);
                        break;
                    }
                }

                //select current recording profile
                for( var i=0; i<self.cameraProfiles().length; i++ )
                {
                    if( self.cameraProfiles()[i].token==self.selectedCamera().record_uri_token )
                    {
                        self.recordingProfile(self.cameraProfiles()[i]);
                        break;
                    }
                }
            })
            .finally(function() {
                self.agocontrol.unblock('#cameraDetails');
            });
        }
        else
        {
            notif.error('Missing information');
        }

    };

    //get camera profiles from scratch (with ip/port/login/pwd)
    self.getCameraProfilesFromScratch = function()
    {
        if( self.cameraIp() && self.cameraPort() && self.cameraLogin() )
        {
            self.agocontrol.block('#configTab');

            //fix password if empty
            if( self.cameraPassword()===null )
            {
                self.cameraPassword('');
            }

            content = {};
            content.uuid = self.controllerUuid;
            content.command = 'getprofiles';
            content.ip = self.cameraIp();
            content.port = self.cameraPort();
            content.login = self.cameraLogin();
            content.password = self.cameraPassword();
            //timeout increased because camera could take time to reply
            self.agocontrol.sendCommand(content, null, 20)
            .then(function(resp) {
                var profiles = [];
                for( i=0; i<resp.data.length; i++ )
                {
                    var profile = {};
                    profile.desc =  resp.data[i].encoding + ' ';
                    profile.desc += resp.data[i].resolution.width + 'x' + resp.data[i].resolution.height;
                    profile.desc += ' @'+ resp.data[i].fps + 'fps';
                    profile.token = resp.data[i].uri_token;
                    profiles.push(profile);
                }
                self.cameraProfiles(profiles);
            })
            .finally(function() {
                self.agocontrol.unblock('#configTab');
            });
        }
        else
        {
            notif.info('Please fill all parameters');
        }
    };

    //delete camera
    self.deleteCamera = function(item, event)
    {
        if( confirm('Really delete camera?') )
        {
            self.agocontrol.block('#agoGrid');

            content = {};
            content.uuid = self.controllerUuid;
            content.command = 'deletecamera';
            content.internalid = item.internalid;
            self.agocontrol.sendCommand(content)
            .then(function(resp) {
                //refresh cameras list
                self.getCameras();
            })
            .finally(function() {
                self.agocontrol.unblock('#agoGrid');
            });
        }
    };

    //open camera details
    self.editCamera = function(item, event)
    {
        //update edit infos
        self.selectedCamera(item);
        self.cameraInternalid(item.internalid);
        self.cameraIp(item.ip);
        self.cameraPort(item.port);
        self.cameraLogin(item.login);
        self.cameraPassword(item.password);
        self.cameraUri(item.motion_uri);
        self.cameraProfiles([]);
        if( item.motion )
        {
            self.motionEnable(true);
        }
        else
        {
            self.motionEnable(false);
        }
        self.motionSensitivity(item.motion_sensitivity);
        self.motionDeviation(item.motion_deviation);
        self.motionOnDuration(item.motion_on_duration);
        var profiles = [];
        var motionP = null;
        if( item.motion_uri_token )
        {
            motionP = {'token':item.motion_uri_token, 'desc':item.motion_uri_desc};
            profiles.push(motionP);
        }
        var recordP = null;
        if( item.record_uri_token )
        {
            if( item.record_uri_token==item.motion_uri_token )
            {
                recordP = motionP;
            }
            else
            {
                recordP = {'token':item.record_uri_token, 'desc':item.record_uri_desc};
                profiles.push(recordP);
            }
        }
        self.cameraProfiles(profiles);
        self.motionProfile(motionP);
        self.recordingProfile(recordP);
        self.recordingType(item.record_type);
        self.recordingDuration(item.record_duration);
        self.recordingContourType(item.record_contour);

        //open modal
        $('#cameraDetails').addClass('active');
    };

    //update camera credentials
    self.updateCameraCredentials = function()
    {
        //don't check password, can be empty by default
        if( self.cameraLogin()!==null && confirm('This action will update credentials on camera too. Continue?') )
        {
            self.agocontrol.block('#cameraDetails');

            //set empty password if null
            if( self.cameraPassword()===null )
            {
                self.cameraPassword('');
            }

            content = {};
            content.uuid = self.controllerUuid;
            content.command = 'updatecredentials';
            content.internalid = self.selectedCamera().internalid;
            content.login = self.cameraLogin();
            content.password = self.cameraPassword();
            self.agocontrol.sendCommand(content, null, 20)
            .then(function(resp) {
                //refresh cameras list
                self.getCameras();
            })
            .catch(function(err) {
                //error, revert changes
                self.cameraLogin(self.selectedCamera().login);
                self.cameraPassword(self.selectedCamera().password);
            })
            .finally(function() {
                self.agocontrol.unblock('#cameraDetails');
            });
        }
        else
        {
            notif.info('Please fill all parameters');
        }
    };

    //transform onvif response to flat array
    self.onvifResponseToFlatArray = function(key, obj)
    {
        var out = [];
        if( obj instanceof Array )
        {
            var items = [];
            for( var i=0; i<obj.length; i++ )
            {
                var item = obj[i];
                if( item instanceof Array || item instanceof Object )
                {
                    var deeps = self.onvifResponseToFlatArray('', item);
                    items.push({
                        'key': '['+i+']',
                        'value': deeps,
                        'type': 'array'
                    });
                }
                else
                {
                    items.push({
                        'key': key,
                        'value': obj[i],
                        'type': 'basic'
                    });
                }
            }
            out.push({
                'key': key,
                'value': items,
                'type': 'array'
            });
        }
        else if( obj instanceof Object )
        {
            for( var k in obj )
            {
                var item = obj[k];
                if( item instanceof Array || item instanceof Object )
                {
                    var deeps = self.onvifResponseToFlatArray(key+'/'+k, item);
                    for( var i=0; i<deeps.length; i++ )
                    {
                        out.push(deeps[i]);
                    }
                }
                else
                {
                    out.push({
                        'key': key+'/'+k,
                        'value': obj[k],
                        'type': 'basic'
                    });
                }
            }
        }
        return out;
    };

    self._addMemberToObject = function(obj, keys, value)
    {
        if( keys.length==1 )
        {
            if( !obj[keys[0]] )
                obj[keys[0]] = value;
        }
        else if( keys.length==2 )
        {
            if( !obj[keys[0]] )
                obj[keys[0]] = {};
            if( !obj[keys[0]][keys[1]] )
                obj[keys[0]][keys[1]] = value;
        }
        else if( keys.length==3 )
        {
            if( !obj[keys[0]] )
                obj[keys[0]] = {};
            if( !obj[keys[0]][keys[1]] )
                obj[keys[0]][keys[1]] = {};
            if( !obj[keys[0]][keys[1]][keys[2]] )
                obj[keys[0]][keys[1]][keys[2]] = value;
        }
        else if( keys.length==4 )
        {
            if( !obj[keys[0]] )
                obj[keys[0]] = {};
            if( !obj[keys[0]][keys[1]] )
                obj[keys[0]][keys[1]] = {};
            if( !obj[keys[0]][keys[1]][keys[2]] )
                obj[keys[0]][keys[1]][keys[2]] = {};
            if( !obj[keys[0]][keys[1]][keys[2]][keys[3]] )
                obj[keys[0]][keys[1]][keys[2]][keys[3]] = value;
        }
        else if( keys.length==5 )
        {
            if( !obj[keys[0]] )
                obj[keys[0]] = {};
            if( !obj[keys[0]][keys[1]] )
                obj[keys[0]][keys[1]] = {};
            if( !obj[keys[0]][keys[1]][keys[2]] )
                obj[keys[0]][keys[1]][keys[2]] = {};
            if( !obj[keys[0]][keys[1]][keys[2]][keys[3]] )
                obj[keys[0]][keys[1]][keys[2]][keys[3]] = {};
            if( !obj[keys[0]][keys[1]][keys[2]][keys[3]][keys[4]] )
                obj[keys[0]][keys[1]][keys[2]][keys[3]][keys[4]] = value;
        }
        else
        {
            console.error('Too many keys!');
            console.error(keys);
        }
    };

    //transform flat array to onvif request parameters
    self.flatArrayToOnvifRequest = function(_array)
    {
        params = {};
        for(var i=0; i<_array.length; i++)
        {
            var key = _array[i].key;
            var value = _array[i].value;
            
            var keys = key.substring(1).split('/');
            self._addMemberToObject(params, keys, value);
        }
        return params;
    };

    //execute onvif operation
    self.doOperation = function(variable, service, operation, params, returnRaw, stringify)
    {
        if( service && operation && params )
        {
            self.agocontrol.block('#cameraDetails');

            content = {};
            content.uuid = self.controllerUuid;
            content.command = 'dooperation';
            content.internalid = self.selectedCamera().internalid;
            content.service = service;
            content.operation = operation;
            content.params = params;
            self.agocontrol.sendCommand(content, null, 20)
            .then(function(resp) {
                var params = resp.data;
                if( !returnRaw )
                {  
                    params = self.onvifResponseToFlatArray('', resp.data);
                }

                if( variable )
                {
                    if( stringify )
                    {
                        variable( JSON.stringify(params, null, 4));
                    }
                    else
                    {
                        variable(params);
                    }
                }
            })
            .finally(function() {
                self.agocontrol.unblock('#cameraDetails');
            });
        }
        else
        {
            notif.error('Parameter is missing');
        }
    };

    //set operation from config tab
    self.setConfigOperation = function()
    {
            self.agocontrol.block('#cameraDetails');

            //build request params
            params = self.flatArrayToOnvifRequest(self.configData());

            content = {};
            content.uuid = self.controllerUuid;
            content.command = 'dooperation';
            content.internalid = self.selectedCamera().internalid;
            content.service = self.configSetService();
            content.operation = self.configSetOperation();
            content.params = params;
            self.agocontrol.sendCommand(content, null, 20)
            .then(function(resp) {
                //nothing to do
            })
            .finally(function() {
                self.agocontrol.unblock('#cameraDetails');
            });
    };

    //get operation from config tab
    self.getConfigOperation = function(variable, service, getOperation, parameters, setOperation)
    {
        self.doOperation(variable, service, getOperation, parameters);
        self.configSetService(service);
        self.configSetOperation(setOperation);
    };

    //get operation from advanced tab
    self.getAdvancedOperation = function()
    {
        if( self.onvifService() && self.onvifOperation() )
        {
            self.agocontrol.block('#cameraDetails');
            self.onvifParameters('');

            content = {};
            content.uuid = self.controllerUuid;
            content.command = 'dooperation';
            content.internalid = self.selectedCamera().internalid;
            content.service = self.onvifService();
            content.operation = self.onvifOperation();
            content.params = {};
            self.agocontrol.sendCommand(content, null, 20)
            .then(function(resp) {
                try
                {
                    self.onvifParameters(JSON.stringify(resp.data, null, 4));
                }
                catch(err)
                {
                    notif.error('Response is invalid ['+err+']');
                }
            })
            .finally(function() {
                self.agocontrol.unblock('#cameraDetails');
            });
        }
        else
        {
            notif.info('Please fill service and operation parameters');
        }
    };

    //check json
    self.checkJson = function()
    {
        try
        {
            JSON.parse(self.onvifParameters());
            notif.success('Parameters syntax is valid');
        }
        catch(err)
        {
            console.error(err);
            notif.warning('Parameters syntax is not valid: '+err);
        }
    };

    //set motion
    self.setMotion = function()
    {
        if( self.motionSensitivity()!==null && self.motionDeviation()!==null && self.motionOnDuration()!==null )
        {
            self.agocontrol.block('#cameraDetails');

            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'setmotion';
            content.internalid = self.selectedCamera().internalid;
            content.enable = self.motionEnable();
            content.uri_token = self.motionProfile().token;
            content.uri_desc = self.motionProfile().desc;
            content.sensitivity = self.motionSensitivity();
            content.deviation = self.motionDeviation();
            content.onduration = self.motionOnDuration();
            self.agocontrol.sendCommand(content, null, 10)
            .then(function(resp) {
                //nothing to do
            })
            .finally(function() {
                self.agocontrol.unblock('#cameraDetails');
            });
        }
        else
        {
            notif.error('Parameter is missing');
        }
    };

    //set recording
    self.setRecording = function()
    {
        if( true /*nothing to check for now*/ )
        {
            self.agocontrol.block('#cameraDetails');

            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'setrecording';
            content.internalid = self.selectedCamera().internalid;
            content.type = self.recordingType();
            content.uri_token = self.recordingProfile().token;
            content.uri_desc = self.recordingProfile().desc;
            content.duration = self.recordingDuration();
            content.contour = self.recordingContourType();
            self.agocontrol.sendCommand(content, null, 10)
            .then(function(resp) {
                //nothing to do
            })
            .finally(function() {
                self.agocontrol.unblock('#cameraDetails');
            });
        }
        else
        {
            notif.error('Parameter is missing');
        }
    };

    //save recordings main configuration
    self.saveRecordingsConfig = function()
    {
        if( $.trim(self.recordingsDir()).length>0 )
        {
            self.agocontrol.block('#configTab');

            var content = {};
            content.uuid = self.controllerUuid;
            content.command = 'setrecordingconfig';
            content.dir = self.recordingsDir();
            content.delay = self.recordingsDelay();
            self.agocontrol.sendCommand(content, null, 10)
            .then(function(resp) {
                //nothing to do
            })
            .finally(function() {
                self.agocontrol.unblock('#configTab');
            });
        }
        else
        {
            notif.error('Parameter is missing');
        }
    };

    //download selected recording
    self.downloadRecording = function(item, event)
    {
        downloadurl = location.protocol + "//" + location.hostname + (location.port && ":" + location.port) + "/download?filename="+item.filename+"&uuid="+self.controllerUuid;
        window.open(downloadurl, '_blank');
    };

    //by default get config
    self.getConfig();
}

/**
 * Entry point: mandatory!
 */
function init_template(path, params, agocontrol)
{
    var model = new OnVIFPlugin(agocontrol.devices(), agocontrol);
    return model;
}
