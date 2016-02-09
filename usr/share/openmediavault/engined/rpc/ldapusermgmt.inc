<?php
/**
 * This file is part of OpenMediaVault.
 *
 * @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
 * @author    Volker Theile <volker.theile@openmediavault.org>
 *                          <fatogre500@gmail.com>
 * @copyright Copyright (c) 2009-2015 Volker Theile
 *
 * OpenMediaVault is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * OpenMediaVault is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenMediaVault. If not, see <http://www.gnu.org/licenses/>.
 */
require_once("openmediavault/object.inc");
require_once("openmediavault/config.inc");
require_once("openmediavault/util.inc");
require_once("openmediavault/functions.inc");
require_once("openmediavault/error.inc");
require_once("openmediavault/system.inc");
require_once("openmediavault/rpcservice.inc");
require_once("openmediavault/notify.inc");

class OMVRpcServiceLdapUserMgmt extends OMVRpcServiceAbstract {
	const LDAP_USERS_PRIMARY_GROUP = 'ldapusers';
   
   /**
	 * Get the RPC service name.
	 */
	public function getName() {
		return "LdapUserMgmt";
	}

	/**
	 * Initialize the RPC service.
	 */
	public function initialize() {
      $this->registerMethod("enumerateLdapUsers");
      $this->registerMethod("enumerateLdapGroups");
		$this->registerMethod("getUserList");
		$this->registerMethod("getUser");
		$this->registerMethod("setUser");
		$this->registerMethod("deleteUser");
		$this->registerMethod("getGroupList");
		$this->registerMethod("getGroup");
		$this->registerMethod("setGroup");
		$this->registerMethod("deleteGroup");
	}
   
   /**
    * Connect to the local LDAP server using the provided root bind DN and
    * root bind password
    * @param rootbinddn the Root Bind DN of the LDAP server
    * @param rootbindpw the Root Bind DN password
    * @return the LDAP connection object
    */
   private function ldapConnectLocal($rootbinddn,$rootbindpw){
      //connect to the ldap server
      if(FALSE === ($ldaplink = ldap_connect("localhost"))){
         throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,
            gettext("Failed to connect to local LDAP server"));            
      }else{
         //use ldapv3 (default is v2)
         ldap_set_option($ldaplink,LDAP_OPT_PROTOCOL_VERSION,3);
      }
      
      //bind using the root dn and password
      if(FALSE === (ldap_bind($ldaplink,$rootbinddn,$rootbindpw))){
         throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,
           gettext("Failed to Bind to local LDAP server using dn: $rootbinddn"));     
      }
      
      return $ldaplink;
   }
   
   /**
    * Close the provided LDAP connection object.
    * @param ldaplink The LDAP connection object
    */
   private function ldapClose($ldaplink){
       //unbind and close ldap connection
      if(FALSE === ldap_unbind($ldaplink)){
         throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,
           gettext("Failed to Unbind connection to local LDAP server"));
      }
   }
   
   /**
    * Modify User Attributes in local LDAP server
    * This function is used to modify an existing entry
    * @param ldaplink The LDAP connection object
    * @param objectdn the DN to modify
    * @param modification an array with keys set to the attribute
    *    to be modified.
    * @return boolean indicated whether the modification was successful
    * @throw  E_MISC_FAILURE
    */
   private function ldapModifyObject($ldaplink,$objectdn,$modifications){
      //make modification to user attribute
      return ldap_modify($ldaplink,$objectdn,$modifications);
   }
   
   /**
    * This function searches the LDAP server based on the provided
    * filter and attributes.
    * @param ldaplink The LDAP connection object
    * @param basedn The base DN of the LDAP server
    * @param filter The filter to use when searching the LDAP Directory
    * @attrs The attributes to search for
    * @return an array with the results of the search
    */
   private function ldapSearch($ldaplink,$basedn,$filter,$attrs){      
      $query=ldap_search($ldaplink,$basedn,$filter,$attrs);
      
      $result=ldap_get_entries($ldaplink,$query);
      
      return $result;      
   }
   
   /**
    * This function checks if the provided uid is a user inside the LDAP directory
    * @param uid The user id to check
    * @return TRUE if the user is in the LDAP Directory, FALSE otherwise
    **/
   private function isLdapUser($uid){
      global $xmlConfig;     
      //get a list of ldap UID Numbers
      //we get the connection info from the config
      $xpath = "//services/ldapserver";
		$object = $xmlConfig->get($xpath);
      $rootbinddn=$object['rootbindcn'].",".$object['basedn'];
      $ldapLink = $this->ldapConnectLocal($rootbinddn,$object['rootbindpw']);
      $queryResults=$this->ldapSearch($ldapLink,$object['basedn'],"objectClass=posixAccount",array("uidnumber"));
      for($i=0;$i<$queryResults['count'];$i++){
         $ldapUserUids[]=$queryResults[$i]["uidnumber"][0];
      }
      $this->ldapClose($ldapLink);
      
      return in_array($uid,$ldapUserUids);
   }
   
   /**
    * This function checks if the provided gid is a group inside the LDAP directory
    * @param gid The group id to check
    * @return TRUE if the group is in the LDAP Directory, FALSE otherwise
    **/
   private function isLdapGroup($gid){
      global $xmlConfig;
      //get a list of ldap GIDs
      //we get the connection info from the config
      $xpath = "//services/ldapserver";
		$object = $xmlConfig->get($xpath);
      $rootbinddn=$object['rootbindcn'].",".$object['basedn'];
      $ldapLink = $this->ldapConnectLocal($rootbinddn,$object['rootbindpw']);
      $queryResults=$this->ldapSearch($ldapLink,$object['basedn'],"(objectClass=posixGroup)",array("gidnumber"));
      for($i=0;$i<$queryResults['count'];$i++){
         $ldapGroupGids[]=$queryResults[$i]["gidnumber"][0];
      }
      $this->ldapClose($ldapLink);
      
      return in_array($gid,$ldapGroupGids);
   }
   
	/**
	 * Enumerate users.
	 * @param type The user type, e.g. system, normal or all.
	 * @return An array containing user objects with following fields:
	 *   name, UID, GID, comment, home directory, and shell program,
	 *   last changed, minimum, maximum, warn, inactive, expire and
	 *   reserved.
	 * @throw E_MISC_FAILURE
	 */
	function enumerateLdapUsers() {
      global $xmlConfig;
      
		// Get list of user names.
		if (FALSE === ($users = OMVUsers::enumerate())) {
			throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,
			  gettext("Failed to get list of users."));
		}
      
      //get a list of ldap UID Numbers
      //we get the connection info from the config
      $xpath = "//services/ldapserver";
		$object = $xmlConfig->get($xpath);
      $rootbinddn=$object['rootbindcn'].",".$object['basedn'];
      $ldapLink = $this->ldapConnectLocal($rootbinddn,$object['rootbindpw']);
      $queryResults=$this->ldapSearch($ldapLink,$object['basedn'],"(objectClass=posixAccount)",array("uidnumber"));
      for($i=0;$i<$queryResults['count'];$i++){
         $ldapUserUids[]=$queryResults[$i]["uidnumber"][0];
      }
      $this->ldapClose($ldapLink);
      
		// Iterate through all users to filter the ldap ones.
		$result = array();
		foreach ($users as $userk => $userv) {
			$user = new OMVUser($userv);
			if (!$user->exists())
				continue;
			// Append user 'root'?
			if (0 == strcasecmp("root", $user->getName()) && FALSE == boolvalEx(
			  $GLOBALS['OMV_USERMGMT_ENUMERATE_USER_ROOT']))
				continue;
			// Check if the current user is requested.
         $append = in_array($user->getUid(),$ldapUserUids);
			$system = $user->isSystemAccount();

			if (TRUE === $append) {
				$result[] = array(
					"name" => $user->getName(),
					"uid" => $user->getUid(),
					"gid" => $user->getGid(),
					"comment" => $user->getGecos(),
					"dir" => $user->getHomeDirectory(),
					"shell" => $user->getShell(),
					"groups" => $user->getGroups(),
					"system" => $system
				);
			}
		}
		return $result;
	}

	/**
	 * Enumerate groups.
	 * @param type The group type, e.g. system, normal or all.
	 * @return An array containing user objects with following fields:
	 *   name, GID and group members (array of user names).
	 * @throw E_MISC_FAILURE
	 */
	function enumerateLdapGroups() {
      global $xmlConfig;
		// Get list of group names.
		if (FALSE === ($groups = OMVUserGroups::enumerate())) {
			throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,
			  gettext("Failed to get list of groups."));
		}
      
      //get a list of ldap GIDs
      //we get the connection info from the config
      $xpath = "//services/ldapserver";
		$object = $xmlConfig->get($xpath);
      $rootbinddn=$object['rootbindcn'].",".$object['basedn'];
      $ldapLink = $this->ldapConnectLocal($rootbinddn,$object['rootbindpw']);
      $queryResults=$this->ldapSearch($ldapLink,$object['basedn'],"(objectClass=posixGroup)",array("gidnumber","cn"));
      for($i=0;$i<$queryResults['count'];$i++){
         //don't display the LDAP_USERS_PRIMARY_GROUP
         if(strcmp(self::LDAP_USERS_PRIMARY_GROUP,$queryResults[$i]["cn"][0])){
            $ldapGroupGids[]=$queryResults[$i]["gidnumber"][0];
         }
      }
      $this->ldapClose($ldapLink);
      
		// Get group information.
		$result = array();
		foreach ($groups as $groupk => $groupv) {
			$group = new OMVUserGroup($groupv);
			if (!$group->exists())
				continue;
			// Check if the current group is requested
			$append = in_array($group->getGid(),$ldapGroupGids);
			$system = $group->isSystemAccount();
			
			if (TRUE === $append) {
				$result[] = array(
					"name" => $group->getName(),
					"gid" => $group->getGid(),
					"members" => $group->getMembers(),
					"system" => $system
				);
			}
		}
		return $result;
	}

	/**
	 * Get list of users (except system users).
	 * @param params An array containing the following fields:
	 *   \em start The index where to start.
	 *   \em limit The number of objects to process.
	 *   \em sortfield The name of the column used to sort.
	 *   \em sortdir The sort direction, ASC or DESC.
	 * @param context The context of the caller.
	 * @return An array containing the requested objects. The field \em total
	 *   contains the total number of objects, \em data contains the object
	 *   array. An exception will be thrown in case of an error.
	 */
	function getUserList($params, $context) {
		global $xmlConfig;
		// Validate the RPC caller context.
		$this->validateMethodContext($context, array(
			  "role" => OMV_ROLE_ADMINISTRATOR
		  ));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "start":{"type":"integer"},
				  "limit":{'.$GLOBALS['OMV_JSONSCHEMA_COUNTFIELD'].'},
				  "sortfield":{'.$GLOBALS['OMV_JSONSCHEMA_SORTFIELD'].'},
				  "sortdir":{'.$GLOBALS['OMV_JSONSCHEMA_SORTDIR'].'}
			  }
		  }');
        
      $xpath = "//services/ldapserver";
		$object = $xmlConfig->get($xpath);
		
      // Get the list of ldap users if ldap server is enabled else return an empty array.
      if($object['enable']){
         $users = $this->enumerateLdapUsers();
      }else{
         return array();
      }
		// Process users and append additional information stored in
		// the database.      
		foreach ($users as $userk => &$userv) {
			// Get additional information stored in database.
			$xpath = sprintf("//system/usermanagement/users/user[name='%s']",
			  $userv['name']);
			$object = $xmlConfig->get($xpath);
			if (!is_null($object)) {
				$userv['email'] = $object['email'];
				$userv['disallowusermod'] = boolvalEx(
				  $object['disallowusermod']);
				$userv['sshpubkeys'] = empty($object['sshpubkeys']) ?
				  array() : $object['sshpubkeys']['sshpubkey'];
			}
		}
      
		// Filter result.
		return $this->applyFilter($users, $params['start'],
		  $params['limit'], $params['sortfield'], $params['sortdir']);
	}

	/**
	 * Get an user configuration object.
	 * @param params An array containing the following fields:
	 *   \em name The name of the user.
	 * @param context The context of the caller.
	 * @return The requested configuration object.
	 * @throw E_MISC_FAILURE
	 */
	function getUser($params, $context) {
		global $xmlConfig;
		// Validate the RPC caller context.
		$this->validateMethodContext($context, array(
			  "role" => OMV_ROLE_ADMINISTRATOR
		  ));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "name":{"type":"string"}
			  }
		  }');
		// Get the user information.
		$user = new OMVUser($params['name']);
		if (!$user->exists()) {
			throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,
			  sprintf(gettext("User '%s' does not exist"),
			  $params['name']));
		}
		$result = array(
			"name" => $user->getName(),
			"uid" => $user->getUid(),
			"gid" => $user->getGid(),
			"comment" => $user->getGecos(),
			"dir" => $user->getHomeDirectory(),
			"shell" => $user->getShell(),
			"groups" => $user->getGroups(),
			"email" => "",
			"disallowusermod" => FALSE,
			"sshpubkeys" => array()
		);
      
      // Get additional user information stored in directory
      $xpath = "//services/ldapserver";
		$object = $xmlConfig->get($xpath);
      $rootbinddn=$object['rootbindcn'].",".$object['basedn'];
      $ldapLink = $this->ldapConnectLocal($rootbinddn,$object['rootbindpw']);
      $queryResults=$this->ldapSearch($ldapLink,$object['basedn'],"(cn:".$params['name'].")",array("mail"));
      for($i=0;$i<$queryResults['count'];$i++){
         $userEmail=$queryResults[$i]["mail"][0];
      }
      $this->ldapClose($ldapLink);
      $result['email']=$userEmail;
      
		// Get additional user information stored in database.
		$xpath = sprintf("//system/usermanagement/users/user[name='%s']",
		  $result['name']);
		$object = $xmlConfig->get($xpath);
		if (!is_null($object)) {
			//$result['email'] = $object['email'];
			$result['disallowusermod'] = false;
			$result['sshpubkeys'] = empty($object['sshpubkeys']) ?
			  array() : $object['sshpubkeys']['sshpubkey'];
		}
		return $result;
	}

	/**
	 * Set (add/update) a non-system user account.
	 * @param params An array containing the following fields:
	 *   \em name The name of the user.
	 *   \em uid The user ID. This field is optional.
	 *   \em groups A list of groups which the user is a member of as an
	 *     array of strings.
	 *   \em shell The name of the users login shell. This field is optional.
	 *   \em password The plain password to use.
	 *   \em email The users email address.
	 *   \em comment Any text string. This field is optional.
	 *   \em disallowusermod Set to FALSE to disallow the user to modify his
	 *     account.
	 *   \em sshpubkeys The users SSH public keys.
	 * @param context The context of the caller.
	 * @return The stored configuration object.
	 * @throw E_CONFIG_SET_OBJECT_FAILED
	 * @throw E_CONFIG_GET_OBJECT_FAILED
	 * @throw E_EXEC_FAILED
	 * @throw E_MISC_FAILURE
	 */
	function setUser($params, $context) {
		global $xmlConfig;
		// Validate the RPC caller context.
		$this->validateMethodContext($context, array(
			  "role" => OMV_ROLE_ADMINISTRATOR
		  ));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "name":{'.$GLOBALS['OMV_JSONSCHEMA_USERNAME'].'},
				  "uid":{"type":"integer","optional":true},
				  "groups":{
					  "type":"array",
					  "items":{
						  "type":"string"
					  }
				  },
				  "shell":{"type":"string","optional":true},
				  "password":{"type":"string"},
				  "email":{'.$GLOBALS['OMV_JSONSCHEMA_EMAIL'].'},
				  "comment":{"type":"string","optional":true},
				  "disallowusermod":{"type":"boolean"},
				  "sshpubkeys":{
					  "type":"array",
					  "items":{'.$GLOBALS['OMV_JSONSCHEMA_SSHPUBKEY_RFC4716'].'}
				  }
			  }
		  }');
		// Check if the given user is a system account. Abort this attempt.
		$user = new OMVUser($params['name']);
		if ($user->exists() && $user->isSystemAccount()) {
			throw new OMVException(OMVErrorMsg::E_MISC_FAILURE, sprintf(
			  gettext("Unauthorized attempt to modify the system account '%s'"),
			  $params['name']));
		}elseif ($user->exists() && !$this->isLdapUser($user->getUid())){         
         //throw error saying that user already exists as a local or system user
         throw new OMVException(OMVErrorMsg::E_MISC_FAILURE, sprintf(
           gettext("Cannot add account '%s' because a local user with the same name already exists!"),
			  $params['name']));         
      }
      
		// Get additional information stored in database.
		$xpath = sprintf("//system/usermanagement/users/user[name='%s']",
		  $params['name']);
		$object = $xmlConfig->get($xpath);
		// Set configuration object.
		$success = FALSE;
		if (is_null($object)) {
			$object = array(
				"uuid" => OMVUtil::uuid(),
				"name" => $params['name'],
				"email" => $params['email'],
				"disallowusermod" => false,
				"sshpubkeys" => empty($params['sshpubkeys']) ?
				  array() : array("sshpubkey" => $params['sshpubkeys'])
			);
			// Append new configuration object.
			$success = $xmlConfig->set("//system/usermanagement/users",
			  array("user" => $object));
		} else {
			$object['email'] = $params['email'];
			$object['disallowusermod'] = false;
			$object['sshpubkeys'] = empty($params['sshpubkeys']) ?
			  array() : array("sshpubkey" => $params['sshpubkeys']);
			// Update existing configuration object.
			$success = $xmlConfig->replace($xpath, $object);
		}
		if (FALSE === $success)
			throw new OMVException(OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED);
		// Prepare command arguments. Note, a user is always in the
		// group 'ldapusers'.

		// Does the user already exist?
		if (!$user->exists()) {
			// Get user management settings.
			$xpath = "//system/usermanagement/homedirectory";
			$hdsobject = $xmlConfig->get($xpath);
			if (is_null($hdsobject)) {
				throw new OMVException(
				  OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $xpath);
			}
			// Append additional arguments.
			if (TRUE === boolvalEx($hdsobject['enable'])) {
				// Get the absolute shared folder path.
				$sfpath = OMVRpc::exec("ShareMgmt", "getPath", array(
				  "uuid" => $hdsobject['sharedfolderref']), $context);
			}
			// Create a new user
			$cmd = sprintf("export LANG=C; ldapadduser %s %s 2>&1",
			  escapeshellarg($params['name']),self::LDAP_USERS_PRIMARY_GROUP);
			if (0 !== $this->exec($cmd, $output)) {
				throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
				  $cmd, implode("\n", $output));
			}
		}
      // Set User Password, Shell, Email and Comment
      $xpath = "//services/ldapserver";
      $ldapObj = $xmlConfig->get($xpath);
      $rootbinddn=$ldapObj['rootbindcn'].",".$ldapObj['basedn'];
      $userObjectDN="uid=".$params['name'].",".$ldapObj['usersuffix'].",".$ldapObj['basedn'];
      //set password if specified
      if(!empty($params['password'])){
         $encodedPassword = "{SHA}" . sha1( $params['password'], TRUE );
         $info['userPassword']=$encodedPassword;
      }
      $info['gecos']=$params['comment'];
      $info['mail']=$params['email'];
      $info['loginShell']=$params['shell'];
      $ldaplink = $this->ldapConnectLocal($rootbinddn,$ldapObj['rootbindpw']);
      if(!$this->ldapModifyObject($ldaplink,$userObjectDN,$info)){
         throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,
           gettext("Failed to modify user object '$userObjectDN' comment and login shell"));
      }
      
      // Get User's Current Groups
      $filter = '(&(objectClass=posixGroup)(memberUid='.$params['name'].'))';
      $results = $this->ldapSearch($ldaplink,$ldapObj['basedn'],$filter,array("cn"));
      for ($i=0;$i < $results["count"]; $i++){
         $currentGroups[]=$results[$i]["cn"][0];
      }      
      $this->ldapClose($ldaplink);
       
      // Add New Groups
      foreach($params['groups'] as $group){
         if(!in_array($group,$currentGroups)){
            $cmd = sprintf("export LANG=C;ldapaddusertogroup %s %s 2>&1", 
              escapeshellarg($params['name']),$group);
            if (0 !== $this->exec($cmd, $output)) {
               throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
				     $cmd, implode("\n", $output));
            }
         }
      }
      
      // Delete Removed Groups
      foreach ($currentGroups as $group){
         if(!in_array($group,$params['groups'])){
            $cmd = sprintf("export LANG=C;ldapdeleteuserfromgroup %s %s 2>&1", 
              escapeshellarg($params['name']),$group);
            if (0 !== $this->exec($cmd, $output)) {
               throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
                 $cmd, implode("\n", $output));
            }
         }
      }
       
		// Append some more information to the notification object.
		$object = array_merge($object, $params);
		// Notify configuration changes.
		$dispatcher = &OMVNotifyDispatcher::getInstance();
		$dispatcher->notify((!$user->exists()) ?
		  OMV_NOTIFY_CREATE : OMV_NOTIFY_MODIFY,
		  "org.openmediavault.system.usermanagement.users.ldapuser", $object);      
      //$dispatcher->notify((!$user->exists()) ?
      //  OMV_NOTIFY_CREATE : OMV_NOTIFY_MODIFY,
      //  "org.openmediavault.system.usermanagement.users.user", $object);
		// Return the configuration object.
		return $object;      
	}

	/**
	 * Delete a user.
	 * @param params An array containing the following fields:
	 *   \em name The name of the user to delete.
	 * @param context The context of the caller.
	 * @return The deleted configuration object.
	 * @throw E_EXEC_FAILED
	 */
	function deleteUser($params, $context) {
		global $xmlConfig;
		// Validate the RPC caller context.
		$this->validateMethodContext($context, array(
			  "role" => OMV_ROLE_ADMINISTRATOR
		  ));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "name":{"type":"string"}
			  }
		  }');
		// Get the user configuration object (required for the notification
		// event).
		$object = $this->callMethod("getUser", array(
		  "name" => $params['name']), $this->getAdminContext());
		// Get a list of groups the the user currently belongs to
		$xpath = "//services/ldapserver";
      $ldapObj = $xmlConfig->get($xpath);
      $rootbinddn=$ldapObj['rootbindcn'].",".$ldapObj['basedn'];
      $userObjectDN="uid=".$params['name'].",".$ldapObj['usersuffix'].",".$ldapObj['basedn'];
      $ldaplink = $this->ldapConnectLocal($rootbinddn,$ldapObj['rootbindpw']);
      $filter = '(&(objectClass=posixGroup)(memberUid='.$params['name'].'))';
      $results = $this->ldapSearch($ldaplink,$ldapObj['basedn'],$filter,array("cn"));
      for ($i=0;$i < $results["count"]; $i++){
         $currentGroups[]=$results[$i]["cn"][0];
      }      
      $this->ldapClose($ldaplink);
      
      // Remove the user from all groups
      foreach ($currentGroups as $group){
         $cmd = sprintf("export LANG=C;ldapdeleteuserfromgroup %s %s 2>&1", 
           escapeshellarg($params['name']),$group);
         if (0 !== $this->exec($cmd, $output)) {
            throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
              $cmd, implode("\n", $output));
         }
      }
      
      // Delete the user.
      $cmd = sprintf("export LANG=C; ldapdeleteuser %s 2>&1",
		  escapeshellarg($params['name']));
		if(0 !== $this->exec($cmd, $output)) {
			throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
			  $cmd, implode("\n", $output));
		}
      
		// Delete additional user information stored in database.
		$xpath = sprintf("//system/usermanagement/users/user[name='%s']",
		  $params['name']);
		$xmlConfig->delete($xpath);
		// Notify configuration changes.
		$dispatcher = &OMVNotifyDispatcher::getInstance();
		$dispatcher->notify(OMV_NOTIFY_DELETE,
		  "org.openmediavault.system.usermanagement.users.ldapuser", $object);
		// Return the deleted configuration object.
		return $object;
	}

	/**
	 * Get list of groups (except system groups).
	 * @param params An array containing the following fields:
	 *   \em start The index where to start.
	 *   \em limit The number of objects to process.
	 *   \em sortfield The name of the column used to sort.
	 *   \em sortdir The sort direction, ASC or DESC.
	 * @param context The context of the caller.
	 * @return An array containing the requested objects. The field \em total
	 *   contains the total number of objects, \em data contains the object
	 *   array. An exception will be thrown in case of an error.
	 */
	function getGroupList($params, $context) {
		global $xmlConfig;
		// Validate the RPC caller context.
		$this->validateMethodContext($context, array(
			  "role" => OMV_ROLE_ADMINISTRATOR
		  ));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "start":{"type":"integer"},
				  "limit":{'.$GLOBALS['OMV_JSONSCHEMA_COUNTFIELD'].'},
				  "sortfield":{'.$GLOBALS['OMV_JSONSCHEMA_SORTFIELD'].'},
				  "sortdir":{'.$GLOBALS['OMV_JSONSCHEMA_SORTDIR'].'}
			  }
		  }');
      
      $xpath = "//services/ldapserver";
		$object = $xmlConfig->get($xpath);
      
		// Get the list of non-system groups if ldap server is enabled else return an empty array.
		if($object['enable']){
         $groups = $this->enumerateLdapGroups();
      }else{
         return array();
      }
		foreach($groups as $groupk => &$groupv) {
			// Get additional information stored in database.
			$xpath = sprintf("//system/usermanagement/groups/group[name='%s']",
			  $groupv['name']);
			$object = $xmlConfig->get($xpath);
			if(!is_null($object)) {
				$groupv['comment'] = $object['comment'];
			}
		}
		// Filter result.
		return $this->applyFilter($groups, $params['start'],
		  $params['limit'], $params['sortfield'], $params['sortdir']);
	}

	/**
	 * Get a group.
	 * @param params An array containing the following fields:
	 *   \em name The name of the group.
	 * @param context The context of the caller.
	 * @return The requested configuration object.
	 * @throw E_MISC_FAILURE
	 */
	function getGroup($params, $context) {
		global $xmlConfig;
		// Validate the RPC caller context.
		$this->validateMethodContext($context, array(
			  "role" => OMV_ROLE_ADMINISTRATOR
		  ));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "name":{"type":"string"}
			  }
		  }');
		// Get the group information.
		$group = new OMVUserGroup($params['name']);
		if(!$group->exists()) {
			throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,
			  sprintf(gettext("Group '%s' does not exist"),
			  $params['name']));
		}
		$result = array(
			"name" => $group->getName(),
			"gid" => $group->getGid(),
			"members" => $group->getMembers()
		);
		// Get additional information stored in database.
		$xpath = sprintf("//system/usermanagement/groups/group[name='%s']",
		  $result['name']);
		$object = $xmlConfig->get($xpath);
		if (!is_null($object))
			$result['comment'] = $object['comment'];
		return $result;
	}

	/**
	 * Set (add/update) a non-system group account.
	 * @param params An array containing the following fields:
	 *   \em name The name of the group.
	 *   \em gid The group ID. This field is optional.
	 *   \em comment Any text string.
	 *   \em members The group members as an array of user names.
	 * @param context The context of the caller.
	 * @return The stored configuration object.
	 * @throw E_CONFIG_SET_OBJECT_FAILED
	 */
	function setGroup($params, $context) {
		global $xmlConfig;
		// Validate the RPC caller context.
		$this->validateMethodContext($context, array(
			  "role" => OMV_ROLE_ADMINISTRATOR
		  ));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "name":{"type":"string"},
				  "gid":{"type":"integer","optional":true},
				  "comment":{"type":"string"},
				  "members":{
					  "type":"array",
					  "items":{
						  "type":"string"
					  }
				  }
			  }
		  }');
 		// Check if the given group is a system account. Abort this attempt.
		$group = new OMVUserGroup($params['name']);
		if ($group->exists() && $group->isSystemAccount()) {
			throw new OMVException(OMVErrorMsg::E_MISC_FAILURE, sprintf(
			  gettext("Unauthorized attempt to modify the system account '%s'"),
			  $params['name']));
		}elseif ($group->exists() && !$this->isLdapGroup($group->getGid())){
         throw new OMVException(OMVErrorMsg::E_MISC_FAILURE, sprintf(
			  gettext("Cannot add the group '%s' because a local group with the same name already exists!"),
			  $params['name']));
      }
		// Try to get existing configuration object.
		$xpath = sprintf("//system/usermanagement/groups/group[name='%s']",
		  $params['name']);
		$object = $xmlConfig->get($xpath);
		// Set configuration object
		$success = FALSE;
		if (is_null($object)) {
			$object = array(
				"uuid" => OMVUtil::uuid(),
				"name" => $params['name'],
				"comment" => $params['comment']
			);
			// Append object to configuration.
			$success = $xmlConfig->set("//system/usermanagement/groups",
			  array("group" => $object));
		} else {
			$object['comment'] = $params['comment'];
			// Update existing configuration object.
			$success = $xmlConfig->replace("//system/usermanagement/groups/".
			  "group[name='{$params['name']}']", $object);
		}
		if (FALSE === $success) {
			throw new OMVException(OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED);
		}
		// Does the group already exist?
		if (!$group->exists()) {
			// Create new group.
			$cmd = sprintf("export LANG=C; ldapaddgroup %s 2>&1",
			  escapeshellarg($params['name']));
			if(0 !== $this->exec($cmd, $output)) {
				throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
				  $cmd, implode("\n", $output));
			}
		} else {
			// Nothing to do here.
		}
      
      // Get a current list of members in given group
      $xpath = "//services/ldapserver";
      $ldapObj = $xmlConfig->get($xpath);
      $rootbinddn=$ldapObj['rootbindcn'].",".$ldapObj['basedn'];
      $userObjectDN="uid=".$params['name'].",".$ldapObj['usersuffix'].",".$ldapObj['basedn'];
      $filter = '(&(objectClass=posixGroup)(cn='.$params['name'].'))';
      $ldaplink = $this->ldapConnectLocal($rootbinddn,$ldapObj['rootbindpw']);
      $results = $this->ldapSearch($ldaplink,$ldapObj['basedn'],$filter,array("memberuid"));
      for ($i=0;$i < $results["count"]; $i++){
         for ($j=0;$j < $results[$i]['memberuid']['count'];$j++){
            $currentMembers[]=$results[$i]['memberuid'][$j];
         }         
      }      
      $this->ldapClose($ldaplink);
      
      // Add new members to group
      foreach($params['members'] as $member){
         if(!in_array($member,$currentMembers)){
            $cmd = sprintf("export LANG=C;ldapaddusertogroup %s %s 2>&1",
              $member,escapeshellarg($params['name']));
            if (0 !== $this->exec($cmd, $output)) {
               throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
                 $cmd, implode("\n", $output));
            }
         }
      }
      
      // Delete removed members from group
      foreach($currentMembers as $member){
         if(!in_array($member,$params['members'])){
            $cmd = sprintf("export LANG=C;ldapdeleteuserfromgroup %s %s 2>&1",
              $member,escapeshellarg($params['name']));
            if (0 !== $this->exec($cmd, $output)) {
               throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
                 $cmd, implode("\n", $output));
            }
         }
      }
      
      // Process members.
		if (array_key_exists("members", $params)) {
			// Append additional fields to configuration object for
			// the notification event.
			$object = array_merge($object, array(
				"members" => $params['members']
			));   
		}
      
		// Notify configuration changes.
		$dispatcher = &OMVNotifyDispatcher::getInstance();
		$dispatcher->notify(!$group->exists() ?
		  OMV_NOTIFY_CREATE : OMV_NOTIFY_MODIFY,
		  "org.openmediavault.system.usermanagement.groups.ldapgroup", $object);
		// Return the configuration object.
		return $object;
	}

	/**
	 * Delete a group.
	 * @param params An array containing the following fields:
	 *   \em name The name of the group to delete.
	 * @param context The context of the caller.
	 * @return The deleted configuration object.
	 * @throw E_EXEC_FAILED
	 */
	function deleteGroup($params, $context) {
		global $xmlConfig;
		// Validate the RPC caller context.
		$this->validateMethodContext($context, array(
			  "role" => OMV_ROLE_ADMINISTRATOR
		  ));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "name":{"type":"string"}
			  }
		  }');
		// Get the group configuration object (required for the notification
		// event).
		$object = $this->callMethod("getGroup", array(
		  "name" => $params['name']), $context);
      // Delete the group.
      $cmd = sprintf("export LANG=C; ldapdeletegroup %s 2>&1",
        escapeshellarg($params['name']));
		if(0 !== $this->exec($cmd, $output)) {
			throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
			  $cmd, implode("\n", $output));
		}
		// Delete configuration object.
		$xpath = sprintf("//system/usermanagement/groups/group[name='%s']",
		  $params['name']);
		$xmlConfig->delete($xpath);
		// Notify configuration changes.
		$dispatcher = &OMVNotifyDispatcher::getInstance();
		$dispatcher->notify(OMV_NOTIFY_DELETE,
		  "org.openmediavault.system.usermanagement.groups.group", $object);
		// Return the deleted configuration object.
		return $object;
	}
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceLdapUserMgmt());
