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
require_once("openmediavault/config.inc");
require_once("openmediavault/error.inc");
require_once("openmediavault/notify.inc");
require_once("openmediavault/object.inc");
require_once("openmediavault/rpcservice.inc");
require_once("openmediavault/util.inc");
class OMVRpcServiceLDAPServer extends OMVRpcServiceAbstract
{
   const TMP_BACKUP_UPLOAD_FILEPATH = "/tmp/ldap-backup.ldif";
	
   /**
	 * Get the name of the RPC service.
	 *
	 * @return string
	 */	
   public function getName()
	{
		return "LDAPServer";
	}
	
   /**
	 * Initialize the RPC service. The RPC methods are registered in this
	 * function with $this->registerMethod.
	 *
	 * @return void
	 */
	public function initialize()
	{
		$this->registerMethod("getSettings");
		$this->registerMethod("setSettings");
      $this->registerMethod("getLdapContent");
      $this->registerMethod("getToolSettings");
      $this->registerMethod("setToolSettings");
      $this->registerMethod("getRestoreSettings");
      $this->registerMethod("uploadBackupFile");
      $this->registerMethod("restoreLdapServer");
      $this->registerMethod("getClearSettings");
      $this->registerMethod("clearLdapServer");
	}
   
   /**
    * Get the LDAP Server configuration settings.
    * @param params The method parameters.
    * @param context The context of the caller.
    * @return An JSON object containing the fields \em enable, \em basedn,
    *   \em rootbindcn, \em rootbindpw, \em usersuffix, and \em groupsuffix.
    * @throw E_CONFIG_GET_OBJECT_FAILED
    */
	public function getSettings($params, $context)
	{
		// $xmlConfig is needed when reading and writing from the configuration.
		global $xmlConfig;
		
		// Validate the RPC caller context.
		//
		// validateMethodContext takes the currentcontext as the first
		// parameter. The second paramter is the valid context and that can be
		// OMV_ROLE_ADMINISTRATOR, OMV_ROLE_USER or OMV_ROLE_EVERYONE.
		// This is used to make sure that the right user accesses the method.
		$this->validateMethodContext($context, array(
			"role" => OMV_ROLE_ADMINISTRATOR
		));
		
		// Get the configuration object.
      $xpath = "//services/ldapserver";
		$object = $xmlConfig->get($xpath);
		
		// If no data was found, throw an exception and provide the XPath that
		// failed.
		if (is_null($object)) {
			throw new OMVException(
				OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
				$xpath
			);
		}
		
		// Modify result data.
		// boolval and intval converts strings and numbers to their boolean
		// and integer value.
		$object["enable"] = boolval($object["enable"]);
		
		return $object;
	}
   
   /**
    * Set the LDAP Server Configuration Settings
    * @param params The method parameters.
    * @param context The context of the caller.
    * @return An JSON object containing the fields \em enable, \em basedn,
    *   \em rootbindcn, \em rootbindpw, \em usersuffix, and \em groupsuffix.
    * @throw E_CONFIG_SET_OBJECT_FAILED
    **/
	public function setSettings($params, $context)
	{
		global $xmlConfig;
		// Validate the RPC caller context.
		$this->validateMethodContext($context, array(
			  "role" => OMV_ROLE_ADMINISTRATOR
		  ));
		// Validate the parameters of the RPC service method.
		$this->validateMethodParams($params, '{
			  "type":"object",
			  "properties":{
				  "enable":{"type":"boolean"},
				  "basedn":{"type":"string"},
              "rootbindcn":{"type":"string"},
              "rootbindpw":{"type":"string"},
              "usersuffix":{"type":"string"},
              "groupsuffix":{"type":"string"}
			  }
		  }');
		// Prepare the configuration object.
		$object = array(
			"enable" => array_boolval($params, 'enable'),
			"basedn" => trim($params['basedn']),
         "rootbindcn" => trim($params['rootbindcn']),
         "rootbindpw" => trim($params['rootbindpw']),
         "usersuffix" => trim($params['usersuffix']),
         "groupsuffix" => $params['groupsuffix'],
		);
      
      //We need to set the ldap client to use the new ldap server information
      //if the ldap server is enabled
      if($object['enable']){      
         $xpath = "//services/ldap";
         $ldapclientObject = $xmlConfig->get($xpath);
         $ldapclientObject['enable'] = 1; //if server is enabled, client should connect to it
         $ldapclientObject['host'] = "localhost";
         $ldapclientObject['base'] = $object['basedn'];
         $ldapclientObject['rootbinddn'] = $object['rootbindcn'].",".$object['basedn'];
         $ldapclientObject['rootbindpw'] = $object['rootbindpw'];
         $ldapclientObject['usersuffix'] = $object['usersuffix'];
         $ldapclientObject['groupsuffix'] = $object['groupsuffix'];
         if (FALSE === $xmlConfig->replace($xpath, $ldapclientObject)) {
            throw new OMVException(OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED);
         }
      }
      
      // Update the configuration file. If it fails it throws an exception.
      $xpath = "//services/ldapserver";
      if (FALSE === $xmlConfig->replace($xpath, $object)) {
         throw new OMVException(OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED);
      }
      
		// Notify configuration changes.
		//
		// This will notify event listeners such as the service module
		// to perform certain tasks. The most common one is to mark the
		// service as dirty.
      $dispatcher = &OMVNotifyDispatcher::getInstance();
      $dispatcher->notify(OMV_NOTIFY_MODIFY,
         "org.openmediavault.services.ldapserver", $object);

		return $object;
	}
   
   /**
    * Get the contents of the LDAP directory. Note, the RPC does not return the
    * file content itself, instead the file path is returned which is
    * used by generic download RPC mechanism to deliver the file content.
    * @param params An array containing the following fields:
    *   \em id The identifier of the ldap server.
    * @param context The context of the caller.
    * @return An array with the keys \em filename, \em filepath and
    *   \em unlink.
    * @throw E_EXEC_FAILED
    *
    **/
   function getLdapContent($params,$context) {
      global $xmlConfig;
      // Validate the RPC caller context.
      $this->validateMethodContext($context, array(
               "role" => OMV_ROLE_ADMINISTRATOR
      ));
      
      $xpath = "//services/ldapserver";
		$object = $xmlConfig->get($xpath);
      
      //do a dump of ldap directory using slapcat to a temp file and change permissions/ownership
      $tmpFilePath = tempnam(sys_get_temp_dir(),"ldapbackup");      
      $cmd = "echo '#basedn: ".$object['basedn']."' > $tmpFilePath;".
             "echo '#bindcn: ".$object['rootbindcn']."' >> $tmpFilePath;".
             "echo '#usersuffix: ".$object['usersuffix']."' >> $tmpFilePath;".
             "echo '#groupsuffix: ".$object['groupsuffix']."' >> $tmpFilePath;".
             "slapcat -v >> ".$tmpFilePath;
      if(0 !== $this->exec($cmd, $output)) {
			throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
                                 $cmd, implode("\n", $output));
      }      
      chmod($tmpFilePath, 0700);
      chown($tmpFilePath, $GLOBALS['OMV_WEBGUI_FILE_OWNERGROUP_NAME']);
      
      return array(
               "filename" => "ldap_backup.ldif",
               "filepath" => $tmpFilePath,
               "unlink" => TRUE
      );      
   }
   
   /**
    * Reserved for future use in case there are new settings to be added
    * for the Tools section
    **/
   function getToolSettings() {
      /*reserved for future use*/
   }
   
   /**
    * Reserved for future use in case there are new settings to be added
    * for the Tools section
    **/
   function setToolSettings() {
      /*reserved for future use*/
   }
   
   /**
    * Reserved for future use in case there are new settings to be added
    * for the Restore LDAP Tool.
    **/
   function getRestoreSettings() {
      /*reserved for future use*/
   }
   
   /**
    * Upload a ldap backup file for restoration and check to make sure
    * that it is a valid backup file.
    * @param params An array containing the following fields:
    *   \em filename The original name of the file.
    *   \em filepath The path to the uploaded file.
    * @param context The context of the caller.
    * @return None.
    * @throw E_EXEC_FAILED
    * @throw E_MISC_FAILURE
    **/
   function uploadBackupFile($params,$context) {
      // Validate the RPC caller context.
		$this->validateMethodContext($context, array(
			  "role" => OMV_ROLE_ADMINISTRATOR
		));
      copy($params['filepath'],self::TMP_BACKUP_UPLOAD_FILEPATH);
      //chmod(self::TMP_BACKUP_UPLOAD_FILEPATH,"0644");
      
      //now we check if the first four lines of the file are correct
      $fh = fopen(self::TMP_BACKUP_UPLOAD_FILEPATH, 'r');
      if (FALSE === $fh) {
         throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,"Could not open Uploaded File for Validation!");
      }else{
         $valid=array();
         $valid[]=preg_match("/^#basedn: .*/",fgets($fh));
         $valid[]=preg_match("/^#bindcn: .*/",fgets($fh));
         $valid[]=preg_match("/^#usersuffix: .*/",fgets($fh));
         $valid[]=preg_match("/^#groupsuffix: .*/",fgets($fh));
      }
      
      if(in_array(0,$valid)) {
         unlink(self::TMP_BACKUP_UPLOAD_FILEPATH);
         throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,"This does not appear to be a ".
                     "valid LDAP Backup File generated by the Backup Tool. Aborting Restore!");
      }
   }
   
   /**
    * Restore the LDAP server from a backup file uploaded by the user.
    * @params params An array containing the following fields:
    * /em newbindpw The new root bind password to use
    * /em restoreconfirm The confirmation keyword
    * return None.
    **/
   function restoreLdapServer($params,$context) {
      // Validate the RPC caller context.
		$this->validateMethodContext($context, array(
			  "role" => OMV_ROLE_ADMINISTRATOR
		));
      
      $this->deleteLdapServer();
      $this->restoreLdapServerFromFile($context,self::TMP_BACKUP_UPLOAD_FILEPATH,$params['newbindpw']);
      
      unlink(self::TMP_BACKUP_UPLOAD_FILEPATH);
   }
   
   /**
    * Reserved for future use in case there are new settings to be added
    * for the Clear LDAP Tool.
    **/
   function getClearSettings() {
      /*reserved for future use*/
   }
   
   /**
    * Clear the current LDAP and restore the default LDAP server.
    * @params params An array containing the following fields:
    * /em deleteconfirm The confirmation keyword
    * return None.
    **/
   function clearLdapServer($params, $context) {
      $defaultLdapFilePath = "/usr/share/slapd/omv-ldap-default.ldif";
      $defaultLdapPassword = "password";
      // Validate the RPC caller context.
		$this->validateMethodContext($context, array(
			  "role" => OMV_ROLE_ADMINISTRATOR
		));
      
      $this->deleteLdapServer();
      $this->restoreLdapServerFromFile($context,$defaultLdapFilePath,$defaultLdapPassword);
   }

   /**
    * Delete default LDAP server.
    * return None.
    **/
   private function deleteLdapServer() {
      global $xmlConfig;
      $xpath = "//services/ldapserver";
		$object = $xmlConfig->get($xpath);
      //stop the openldap service while we clear the ldap directory
      $cmd = "service slapd stop";
      if (0 !== $this->exec($cmd, $output)) {
         throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
                 $cmd, implode("\n", $output));
      }
      
      //now we clear the ldap directory
      $ldapDir="/var/lib/ldap";
      $ldapConf="/etc/ldap/slapd.d";
      $delCmd="rm -rf $ldapDir/* $ldapConf/cn=config $ldapConf/cn=config.ldif";
      if (0 !== $this->exec($delCmd, $output)) {
         throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
                 $delCmd, implode("\n", $output));
      }
   }
   
   /**
    * Restore the LDAP server from provided parameters.
    * @params filepath The file path of the ldap backup file
    * @params bindpw The root bind password for the LDAP server
    * return None.
    **/
   private function restoreLdapServerFromFile($context,$filepath,$bindpw){
      global $xmlConfig;
      $slapdInitLdif = "/usr/share/slapd/slapd.init.ldif";
      $defaultBackend = "hdb";
      $defaultBackendObjectClass = "olcHdbConfig";
      $ldapDir="/var/lib/ldap";
      $ldapConf="/etc/ldap/slapd.d";
      
      if(!file_exists($filepath)){
         throw new OMVException(OMVErrorMsg::E_MISC_FAILURE, "File not found! $filepath");
      }
      
      //open ldif file for reading and read in first 4 lines
      $fh = fopen($filepath, 'r');
      $line1 = trim(fgets($fh));
      $line2 = trim(fgets($fh));
      $line3 = trim(fgets($fh));
      $line4 = trim(fgets($fh));
      fclose($fh);
      
      //get the ldap server configuration settings from reading the lines
      preg_match('/^#basedn: (.*)/',$line1,$basednMatch); $basedn = $basednMatch[1];
      preg_match('/^#bindcn: (.*)/',$line2,$bindcnMatch); $bindcn = $bindcnMatch[1];
      preg_match('/^#usersuffix: (.*)/',$line3,$userSuffixMatch); $usersuffix=$userSuffixMatch[1];
      preg_match('/^#groupsuffix: (.*)/',$line4,$groupSuffixMatch); $groupsuffix=$groupSuffixMatch[1];
            
      $tmpFilePath = tempnam(sys_get_temp_dir(),"ldaprestore");
      //copy config slapd.init.ldif to temp file location
      if(FALSE === copy($slapdInitLdif, $tmpFilePath)) {
                        throw new OMVException(OMVErrorMsg::E_MISC_FAILURE,
                          sprintf("Failed to copy file (source=%s, dest=%s).",
                          $sladInitLdif, $tmpFilePath));
      }
      
      //replace some parameters inside the init ldif
      $initLdif = file_get_contents($tmpFilePath);
      $initLdif = str_replace('@BACKEND@',$defaultBackend,$initLdif);
      $initLdif = str_replace('@BACKENDOBJECTCLASS@',$defaultBackendObjectClass,$initLdif);
      $initLdif = str_replace('@SUFFIX@',$basedn,$initLdif);
      $initLdif = str_replace('@PASSWORD@',$bindpw,$initLdif);
      $initLdif = str_replace('cn=admin',$bindcn,$initLdif);
      file_put_contents($tmpFilePath,$initLdif);
      
      //slapadd the config
      $cmd = "slapadd -F \"/etc/ldap/slapd.d\" -b \"cn=config\" -l $tmpFilePath";
      if (0 !== $this->exec($cmd, $output)) {
         throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
                 $cmd, implode("\n", $output));
      }
      unlink($tmpFilePath);
      
      //slapadd the directory
      $cmd = "slapadd -b \"$basedn\" -v -l $filepath";
      if (0 !== $this->exec($cmd, $output)) {
         throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
                 $cmd, implode("\n", $output));
      }
      
      //set ownership of files
      $cmd = "chown -R openldap:openldap $ldapDir;chown -R openldap:openldap $ldapConf";
      if (0 !== $this->exec($cmd, $output)) {
         throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
                 $cmd, implode("\n", $output));
      }
      
      //set the omv config settings to the newly restored server settings for both server and client modules
      $xpath = "//services/ldapserver";
		$object = $xmlConfig->get($xpath);
      $object['enable'] = TRUE;
      $object['basedn'] = $basedn;
      $object['rootbindcn'] = $bindcn;
      $object['rootbindpw'] = $bindpw;
      $object['usersuffix'] = $usersuffix;
      $object['groupsuffix'] = $groupsuffix;
      if (FALSE === $xmlConfig->replace($xpath, $object)) {
         throw new OMVException(OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED);
      }
      
      $xpath = "//services/ldap";
      $ldapclientObject = $xmlConfig->get($xpath);
      $ldapclientObject['enable'] = 1; //if server is enabled, client should connect to it
      $ldapclientObject['host'] = "localhost";
      $ldapclientObject['base'] = $object['basedn'];
      $ldapclientObject['rootbinddn'] = $object['rootbindcn'].",".$object['basedn'];
      $ldapclientObject['rootbindpw'] = $object['rootbindpw'];
      $ldapclientObject['usersuffix'] = $object['usersuffix'];
      $ldapclientObject['groupsuffix'] = $object['groupsuffix'];
      if (FALSE === $xmlConfig->replace($xpath, $ldapclientObject)) {
         throw new OMVException(OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED);
      }
      
      //update the ldapscripts.conf file
      copy("/etc/ldapscripts/ldapscripts.default.conf","/etc/ldapscripts/ldapscripts.conf");
      $ldapscriptsConf = file_get_contents("/etc/ldapscripts/ldapscripts.conf");
      $ldapscriptsConf = preg_replace("/SUFFIX.*\n/","SUFFIX=\"$basedn\"\n",$ldapscriptsConf);
      $ldapscriptsConf = preg_replace("/BINDDN.*\n/","BINDDN=\"$bindcn,$basedn\"\n",$ldapscriptsConf);
      $ldapscriptsConf = preg_replace("/USUFFIX.*\n/","USUFFIX=\"$usersuffix\"\n",$ldapscriptsConf);
      $ldapscriptsConf = preg_replace("/GSUFFIX.*\n/","GSUFFIX=\"$groupsuffix\"\n",$ldapscriptsConf);
      file_put_contents("/etc/ldapscripts/ldapscripts.conf",$ldapscriptsConf);
      unlink("/etc/ldapscripts/ldapscripts.passwd");
      touch("/etc/ldapscripts/ldapscripts.passwd");
      chmod("/etc/ldapscripts/ldapscripts.passwd",0640);
      
      //start the ldap service
      $cmd = "service slapd start";
      if (0 !== $this->exec($cmd, $output)) {
         throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
                 $cmd, implode("\n", $output));
      }
      
      // Apply the changes immediately (no user interaction is wanted).
      OMVRpc::exec("Config", "applyChanges", array(
                 "modules" => array("ldap","ldapserver"),
                 "force" => TRUE
      ), $context);
   }
   
}
// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceLDAPServer());
?>