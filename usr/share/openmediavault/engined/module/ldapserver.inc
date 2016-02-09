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
require_once("openmediavault/module.inc");
require_once("openmediavault/config.inc");
require_once("openmediavault/error.inc");

class OMVModuleLDAPServer extends OMVModuleServiceAbstract
  implements OMVINotifyListener, OMVIModuleServiceStatus {
	/**
	 * Get the module name.
	 */
	public function getName() {
		return "ldapserver";
	}

	/**
	 * Generate the configuration.
	 * @throw E_EXEC_FAILED
	 */
	public function applyConfig() {
		$cmd = "omv-mkconf ldapserver 2>&1";
		if(0 !== $this->exec($cmd, $output)) {
			throw new OMVException(OMVErrorMsg::E_EXEC_FAILED,
			  $cmd, implode("\n", $output));
		}
	}

   /**
    * Stop the managed service.
    */
   public function stopService() {
      global $xmlConfig;
      $xpath = "//services/ldapserver";
      $object = $xmlConfig->get($xpath);
      if(is_null($object)) {
         throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
            $xpath);
      }
      if (FALSE === boolvalEx($object['enable'])) {
         $initd = new OMVSysVInitScript("slapd", FALSE);
         $initd->exec();
      }
   }

   /**
    * Start the managed service.
    */
   public function startService() {
      global $xmlConfig;
      $xpath = "//services/ldapserver";
      $object = $xmlConfig->get($xpath);
      if(is_null($object)) {
         throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
            $xpath);
      }
      if (FALSE === boolvalEx($object['enable']))
         return;
      $initd = new OMVSysVInitScript("slapd", TRUE);
      switch($initd->status()) {
         case 0: // running -> reload config
            $initd->invoke("force-reload");
            break;
         default: // start service
            $initd->exec();
      }
   }
   
   /**
    * Get the status of the managed service.
    * @throw E_CONFIG_GET_OBJECT_FAILED
    */
   public function getStatus() {
      global $xmlConfig;
      $xpath = "//services/ldapserver";
      $object = $xmlConfig->get($xpath);
      if(is_null($object)) {
         throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
            $xpath);
      }
      $initd = new OMVSysVInitScript("slapd");
      return array(
         "name" => $this->getName(),
         "title" => gettext("OpenLDAP"),
         "enabled" => boolvalEx($object['enable']),
         "running" => ($initd->status() == 0)
      );
   }
   
	/**
	 * Bind listeners.
	 */
	function bindListeners(OMVNotifyDispatcher $dispatcher) {    
		$dispatcher->addListener(
		  OMV_NOTIFY_MODIFY,
		  "org.openmediavault.services.ldapserver",
		  array($this, "setDirty"));
      $moduleMgr = &OMVModuleMgr::getInstance();
		$dispatcher->addListener(
		  OMV_NOTIFY_MODIFY,
		  "org.openmediavault.services.ldapserver",
		  array($moduleMgr->getModule("ldap"), "setDirty"));
   }
}

// Register module.
$moduleMgr = &OMVModuleMgr::getInstance();
$moduleMgr->registerModule(new OMVModuleLDAPServer());
?>
