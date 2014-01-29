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
    if (file_exists($this->webroot())) {
      exec("rm -rf " . $this->webroot() . '/sites/*');
    }
    $this->setUpDrupal(1, TRUE);

    // For speed, cache the repo we clone.
    $cached_repo = $this->cachedRepo();
    if (!file_exists($cached_repo)) {
      exec("cd " . dirname($cached_repo) . " && git clone --bare " . DEPLOYOTRON_SITE_REPO . " " . basename($cached_repo), $output, $rc);
      if ($rc != 0) {
        $this->fail('Problem cloning site for deployment.');
      }
    }

    // And a 'site' to deploy to.
    $deploy_site = $this->deploySite();
    if (file_exists($deploy_site)) {
      // Start afresh.
      `rm -rf $deploy_site`;
    }
    exec("cd " . dirname($deploy_site) . " && git clone " . $cached_repo . " $deploy_site", $output, $rc);
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

    // Leech on the settings file of the site Drush installed. We need one for
    // the DB setup for the deployment site, and we're not using the 'local'
    // one.
    copy($this->webroot() . '/sites/dev/settings.php', $deploy_site . '/sites/default/settings.php');
    if (file_exists($site_drush)) {
      // Start afresh.
      `rm -rf $site_drush`;
    }
    mkdir($site_drush, 0777, TRUE);

    $this->writeAlias(array(
        'branch' => 'master',
      ));

    // Finally, we need to install the deployotron drush command.
    mkdir($site_drush . '/deployotron', 0777, TRUE);
    $deployotron_dir = dirname(__DIR__);
    // Symlinking files in, in order to play nice with test coverage
    // reporting.
    symlink($deployotron_dir . '/deployotron.drush.inc', $site_drush . '/deployotron/deployotron.drush.inc');
    symlink($deployotron_dir . '/deployotron.actions.inc', $site_drush . '/deployotron/deployotron.actions.inc');
  }

  /**
   * Create the deployotron alias with the given options.
   */
  public function writeAlias($deploy_options) {
    $aliases = array(
      'uri' => 'default',
      'root' => $this->deploySite(),
      'deployotron' => $deploy_options,
    );
    $alias_content = "<?php
\$aliases['deployotron'] = " . var_export($aliases, TRUE) . ';';
    $site_drush = $this->webroot() . '/sites/all/drush';
    file_put_contents($site_drush . '/deployotron.aliases.drushrc.php', $alias_content);
  }

  /**
   * Path to the cached repo.
   */
  public function cachedRepo() {
    return $this->directory_cache('deployotron_drupal_repo');
  }

  /**
   * Path to the faked remote site.
   */
  public function deploySite() {
    return UNISH_SANDBOX . '/deployotron';
  }

  /**
   * Test help commands.
   */
  public function testHelp() {
    // Drush 5 needs to be kicked to see the new command.
    $this->drush('cc', array('drush'), array(), NULL, $this->webroot());

    // Check that the help is available.
    $this->drush('help', array('deploy'), array(), NULL, $this->webroot());
    $this->assertRegExp('/Deploy site to a specific environment/', $this->getOutput());

    // Also for the omg command.
    $this->drush('help', array('omg'), array(), NULL, $this->webroot());
    $this->assertRegExp('/Try to find a backup, and restore it to the site/', $this->getOutput());

    // And check that the topic command outputs something.
    $this->drush('topic', array('deployotron-actions'), array(), NULL, $this->webroot());
    // Check some random strings.
    $this->assertRegExp('/Commands\\n--------/', $this->getOutput());
    $this->assertRegExp('/Actions\\n-------/', $this->getOutput());
    $this->assertRegExp('/deploy runs the actions: SanityCheck/', $this->getOutput());
    $this->assertRegExp('/DeployCode:\\nChecks out a specified/', $this->getOutput());
    $this->assertRegExp('/--branch/', $this->getOutput());

    // Use the filter arg to get full coverage of hook_drush_help().
    $this->drush('help', array(), array('filter' => NULL, 'n' => NULL), NULL, $this->webroot());
    $this->assertRegExp('/Deployotron: Deploys site./', $this->getOutput());
  }

  /**
   * Test for command line argument errors.
   */
  public function testErrors() {
    // Drush 5 needs to be kicked to see the new command.
    $this->drush('cc', array('drush'), array(), NULL, $this->webroot());

    // No alias error message.
    $this->drush('deploy 2>&1', array(), array('y' => TRUE, 'branch' => '', 'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0'), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/No alias given/', $this->getOutput());

    // Bad alias error.
    $this->drush('deploy 2>&1', array('@badalias'), array('y' => TRUE, 'branch' => '', 'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0'), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/Invalid alias/', $this->getOutput());

    // Also check that aborting works.
    $this->drush('deploy 2>&1', array('@deployotron'), array('n' => TRUE, 'branch' => '', 'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0'), NULL, $this->webroot());
    $this->assertRegExp('/Aborting/', $this->getOutput());
    $this->assertNotRegExp('/Done/', $this->getOutput());
  }

  /**
   * Check that the basics work.
   */
  public function testBasic() {
    // Drush 5 needs to be kicked to see the new command.
    $this->drush('cc', array('drush'), array(), NULL, $this->webroot());

    // Check that deployment works.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0'), NULL, $this->webroot());
    // Also check that we get a notice for for having both branch (on alias) and
    // sha (command line) specified.
    $this->assertRegExp('/More than one of branch\\/tag\\/sha specified, using sha./', $this->getOutput());
    $this->assertRegExp('/HEAD now at 04256b5992d8b4a4fae25c7cb7888583749fabc0/', $this->getOutput());

    // Check that VERSION.txt was created.
    $this->assertFileExists($this->deploySite() . '/VERSION.txt');

    // Check that a dirty checkout makes deployment fail..
    file_put_contents($this->deploySite() . '/index.php', 'stuff');
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/Repository not clean/', $this->getOutput());

    // Fix the the checkout and check that we can now deploy, and throw in
    // no-confirm for coverage.
    exec('cd ' . $this->deploySite() . ' && git reset --hard');
    $this->drush('deploy 2>&1', array('@deployotron'), array('no-confirm' => TRUE, 'branch' => '', 'sha' => 'b9471948c3f83a665dd4f106aba3de8962d69b42'), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at b9471948c3f83a665dd4f106aba3de8962d69b42/', $this->getOutput());

    // VERSION.txt should still exist.
    $this->assertFileExists($this->deploySite() . '/VERSION.txt');
    // Check content.
    $version_txt = file_get_contents($this->deploySite() . '/VERSION.txt');
    $this->assertRegExp('/Deployment info/', $version_txt);
    $this->assertRegExp('/SHA: b9471948c3f83a665dd4f106aba3de8962d69b42/', $version_txt);
    $this->assertRegExp('/Time of deployment: /', $version_txt);
    $this->assertRegExp('/Deployer: /', $version_txt);

    // Check that the switch for echo didn't got written to the file.
    $this->assertNotRegExp('/-e/', $version_txt);

    // Check that a file in the way of a new file will cause the deployment to
    // roll back.
    file_put_contents($this->deploySite() . '/sites/all/modules/coffee', 'stuff');
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/Aborting/', $this->getOutput());
    $this->assertRegExp('/Could not checkout code/', $this->getOutput());

    // Check that we rolled back.
    $this->assertRegExp('/Rolled back set site offline/', $this->getOutput());

    // Check that we're still at the same version.
    exec('cd ' . $this->deploySite() . ' && git rev-parse HEAD', $output);
    $this->assertEquals('b9471948c3f83a665dd4f106aba3de8962d69b42', trim(implode('', $output)));

    // Remove the blocker and try again.
    unlink($this->deploySite() . '/sites/all/modules/coffee');
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE), NULL, $this->webroot());
    $version_txt = file_get_contents($this->deploySite() . '/VERSION.txt');
    $this->assertRegExp('/Branch: master/', $version_txt);
    $this->assertRegExp('/SHA: fbcaa29d45716edcbedc3c325bfbab828f1ce838/', $version_txt);
  }

  /**
   * Test configured messages.
   */
  public function testMessages() {
    // Drush 5 needs to be kicked to see the new command.
    $this->drush('cc', array('drush'), array(), NULL, $this->webroot());
    $this->writeAlias(array(
        'branch' => 'master',
        'no-updb' => TRUE,
        'no-cc-all' => TRUE,
        'no-offline' => TRUE,
        'no-dump' => TRUE,
        'message' => 'Those about to deploy, salute you',
      ));

    // Check that messages are printed.
    $options = array(
      'y' => TRUE,
      'branch' => '',
      'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0',
    );
    $this->drush('deploy 2>&1', array('@deployotron'), $options, NULL, $this->webroot());
    $this->assertRegExp('/Those about to deploy, salute you/', $this->getOutput());

    $this->writeAlias(array(
        'branch' => 'master',
        'no-updb' => TRUE,
        'no-cc-all' => TRUE,
        'no-offline' => TRUE,
        'no-dump' => TRUE,
        'confirm_message' => 'Those about to deploy, salute you',
        'done_message' => "Are you pondering what I'm pondering?",
      ));

    // Check that confirm message is printed if aborting, but not done message.
    $options = array(
      'n' => TRUE,
      'branch' => '',
      'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0',
    );
    $this->drush('deploy 2>&1', array('@deployotron'), $options, NULL, $this->webroot());
    $this->assertRegExp('/Those about to deploy, salute you/', $this->getOutput());
    $this->assertNotRegExp("/Are you pondering what I'm pondering/", $this->getOutput());

    // Check that both are printed on successfull deploy.
    $options = array(
      'y' => TRUE,
      'branch' => '',
      'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0',
    );
    $this->drush('deploy 2>&1', array('@deployotron'), $options, NULL, $this->webroot());
    $this->assertRegExp('/Those about to deploy, salute you/', $this->getOutput());
    $this->assertRegExp("/Are you pondering what I'm pondering/", $this->getOutput());
  }
}
