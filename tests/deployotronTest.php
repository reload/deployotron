<?php

/**
 * @file
 * PHPUnit Tests for Deployotron.
 */

define('DEPLOYOTRON_SITE_REPO', 'https://github.com/reload/deployotron_testsite.git');

/**
 * Deployotron testing class.
 */
class DrakeCase extends Drush_CommandTestCase {
  /**
   * Setup before each test case.
   */
  public function setUp() {
    // Deployotron needs a site to run in.
    $this->setUpDrupal();

    // And a 'site' to deploy to.
    $deploy_site = $this->deploySite();
    if (file_exists($deploy_site)) {
      // Start afresh.
      `rm -rf $deploy_site`;
    }
    exec("cd " . dirname($deploy_site) . " && git clone " . DEPLOYOTRON_SITE_REPO . " $deploy_site", $output, $rc);
    if ($rc != 0) {
      $this->fail('Problem cloning site for deployment.');
    }
    // Check out a known version.
    exec("cd " . $deploy_site . " && git checkout 7a9166ac76bb63f45a5a8a6b9a4e3a58eb04da6e");

    // Pretend that the current site is a clone of the same. Local git commands
    // need this.
    exec('cp -r ' . $deploy_site . '/.git ' . $this->webroot());

    // We'll need to bind them together with an alias file in the 'local' site.
    $site_drush = $this->webroot() . '/sites/all/drush';
    if (file_exists($site_drush)) {
      // Start afresh.
      `rm -rf $site_drush`;
    }
    mkdir($site_drush, 0777, TRUE);
    $alias_content = "<?php
\$aliases['deployotron'] = array(
  'uri' => 'default',
  'root' => '" . $this->deploySite() . "',
  'deployotron' => array(
    'branch' => 'master',
    'no-updb' => TRUE,
    'no-cc-all' => TRUE,
    'no-offline' => TRUE,
    'no-dump' => TRUE,
  ),
);
";
    file_put_contents($site_drush . '/deployotron.aliases.drushrc.php', $alias_content);

    // Finally, we need to install the deployotron drush command.
    mkdir($site_drush . '/deployotron', 0777, TRUE);
    $deployotron_dir = dirname(__DIR__);
    // Using a wildcard to skip hidden files (.git, etc).
    `cp -r $deployotron_dir/* $site_drush/deployotron`;
  }

  /**
   * Path to the faked remote site.
   */
  public function deploySite() {
    return UNISH_SANDBOX . '/deployotron';
  }

  /**
   * Check that the basics work.
   */
  public function testBasic() {
    // Simple test to see if the deploy command is available.
    // Drush 5 needs to be kicked to see the new command.
    $this->drush('cc', array('drush'), array(), NULL, $this->webroot());
    $this->drush('help', array('deploy'), array(), NULL, $this->webroot());
    $this->assertRegExp('/Deploy site to a specific environment/', $this->getOutput());

    // Check that deployment works.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'branch' => '', 'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0'), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at 04256b5992d8b4a4fae25c7cb7888583749fabc0/', $this->getOutput());

    // Check that a dirty checkout makes deployment fail..
    file_put_contents($this->deploySite() . '/index.php', 'stuff');
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/Repository not clean/', $this->getOutput());

    // Fix the the checkout and check that we can now deploy.
    exec('cd ' . $this->deploySite() . ' && git reset --hard');
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at b9471948c3f83a665dd4f106aba3de8962d69b42/', $this->getOutput());

    // @todo check that a file in the way of a new file will cause the
    //   deployment to roll back

    // $this->log($this->getOutput());
  }
}
