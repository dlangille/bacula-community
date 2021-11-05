<?php

/**
 * TReCaptcha class file
 *
 * @author Bérczi Gábor <gabor.berczi@devworx.hu>
 * @link http://www.devworx.hu/
 * @copyright Copyright &copy; 2011 DevWorx
 * @license https://github.com/pradosoft/prado/blob/master/LICENSE
 * @package Prado\Web\UI\WebControls
 */

namespace Prado\Web\UI\WebControls;

use Prado\Exceptions\TConfigurationException;
use Prado\TPropertyValue;
use Prado\Web\Javascripts\TJavaScript;
use Prado\Web\Javascripts\TJavaScriptLiteral;

/**
 * TReCaptcha class.
 *
 * TReCaptcha displays a reCAPTCHA (a token displayed as an image) that can be used
 * to determine if the input is entered by a real user instead of some program. It can
 * also prevent multiple submits of the same form either by accident, or on purpose (ie. spamming).
 *
 * The reCAPTCHA to solve (a string consisting of two separate words) displayed is automatically
 * generated by the reCAPTCHA system at recaptcha.net. However, in order to use the services
 * of the site you will need to register and get a public and a private API key pair, and
 * supply those to the reCAPTCHA control through setting the {@link setPrivateKey PrivateKey}
 * and {@link setPublicKey PublicKey} properties.
 *
 * Currently the reCAPTCHA API supports only one reCAPTCHA field per page, so you MUST make sure that all
 * your input is protected and validated by a single reCAPTCHA control. Placing more than one reCAPTCHA
 * control on the page will lead to unpredictable results, and the user will most likely unable to solve
 * any of them successfully.
 *
 * Upon postback, user input can be validated by calling {@link validate()}.
 * The {@link TReCaptchaValidator} control can also be used to do validation, which provides
 * server-side validation. Calling (@link validate()) will invalidate the token supplied, so all consecutive
 * calls to the method - without solving a new captcha - will return false. Therefore if implementing a multi-stage
 * input process, you must make sure that you call validate() only once, either at the end of the input process, or
 * you store the result till the end of the processing.
 *
 * The following template shows a typical use of TReCaptcha control:
 * <code>
 * <com:TReCaptcha ID="Captcha"
 *                 PublicKey="..."
 *                 PrivateKey="..."
 * />
 * <com:TReCaptchaValidator ControlToValidate="Captcha"
 *                          ErrorMessage="You are challenged!" />
 * </code>
 *
 * @author Bérczi Gábor <gabor.berczi@devworx.hu>
 * @package Prado\Web\UI\WebControls
 * @since 3.2
 */
class TReCaptcha extends \Prado\Web\UI\WebControls\TWebControl implements \Prado\Web\UI\IValidatable
{
	private $_isValid = true;

	const ChallengeFieldName = 'recaptcha_challenge_field';
	const ResponseFieldName = 'recaptcha_response_field';
	const RECAPTCHA_API_SERVER = "http://www.google.com/recaptcha/api";
	const RECAPTCHA_API_SECURE_SERVER = "https://www.google.com/recaptcha/api";
	const RECAPTCHA_VERIFY_SERVER = "www.google.com";
	const RECAPTCHA_JS = 'http://www.google.com/recaptcha/api/js/recaptcha_ajax.js';

	public function getTagName()
	{
		return 'span';
	}

	/**
	 * Returns true if this control validated successfully.
	 * Defaults to true.
	 * @return bool wether this control validated successfully.
	 */
	public function getIsValid()
	{
		return $this->_isValid;
	}
	/**
	 * @param bool $value wether this control is valid.
	 */
	public function setIsValid($value)
	{
		$this->_isValid = TPropertyValue::ensureBoolean($value);
	}

	public function getValidationPropertyValue()
	{
		return $this->Request[$this->getChallengeFieldName()];
	}

	public function getPublicKey()
	{
		return $this->getViewState('PublicKey');
	}

	public function setPublicKey($value)
	{
		return $this->setViewState('PublicKey', TPropertyValue::ensureString($value));
	}

	public function getPrivateKey()
	{
		return $this->getViewState('PrivateKey');
	}

	public function setPrivateKey($value)
	{
		return $this->setViewState('PrivateKey', TPropertyValue::ensureString($value));
	}

	public function getThemeName()
	{
		return $this->getViewState('ThemeName');
	}

	public function setThemeName($value)
	{
		return $this->setViewState('ThemeName', TPropertyValue::ensureString($value));
	}

	public function getCustomTranslations()
	{
		return TPropertyValue::ensureArray($this->getViewState('CustomTranslations'));
	}

	public function setCustomTranslations($value)
	{
		return $this->setViewState('CustomTranslations', TPropertyValue::ensureArray($value));
	}

	public function getLanguage()
	{
		return $this->getViewState('Language');
	}

	public function setLanguage($value)
	{
		return $this->setViewState('Language', TPropertyValue::ensureString($value));
	}

	public function getCallbackScript()
	{
		return $this->getViewState('CallbackScript');
	}

	public function setCallbackScript($value)
	{
		return $this->setViewState('CallbackScript', TPropertyValue::ensureString($value));
	}

	protected function getChallengeFieldName()
	{
		return /*$this->ClientID.'_'.*/self::ChallengeFieldName;
	}

	public function getResponseFieldName()
	{
		return /*$this->ClientID.'_'.*/self::ResponseFieldName;
	}

	public function getClientSideOptions()
	{
		$options = [];
		if ($theme = $this->getThemeName()) {
			$options['theme'] = $theme;
		}
		if ($lang = $this->getLanguage()) {
			$options['lang'] = $lang;
		}
		if ($trans = $this->getCustomTranslations()) {
			$options['custom_translations'] = $trans;
		}
		return $options;
	}

	public function validate()
	{
		if (!
			  (
			($challenge = @$_POST[$this->getChallengeFieldName()])
			and
			($response = @$_POST[$this->getResponseFieldName()])
			  )
				   ) {
			return false;
		}

		return $this->recaptcha_check_answer(
			$this->getPrivateKey(),
			$_SERVER["REMOTE_ADDR"],
			$challenge,
			$response
		);
	}

	/**
	 * Checks for API keys
	 * @param mixed $param event parameter
	 */
	public function onPreRender($param)
	{
		parent::onPreRender($param);

		if ("" == $this->getPublicKey()) {
			throw new TConfigurationException('recaptcha_publickey_unknown');
		}
		if ("" == $this->getPrivateKey()) {
			throw new TConfigurationException('recaptcha_privatekey_unknown');
		}

		// need to register captcha fields so they will be sent back also in callbacks
		$page = $this->getPage();
		$page->registerRequiresPostData($this->getChallengeFieldName());
		$page->registerRequiresPostData($this->getResponseFieldName());
	}

	protected function addAttributesToRender($writer)
	{
		parent::addAttributesToRender($writer);
		$writer->addAttribute('id', $this->getClientID());
	}

	public function regenerateToken()
	{
		// if we're in a callback, then schedule re-rendering of the control
		// if not, don't do anything, because a new challenge will be rendered anyway
		if ($this->Page->IsCallback) {
			$this->Page->CallbackClient->jQuery($this->getClientID() . ' #recaptcha_reload', 'click');
		}
	}

	public function renderContents($writer)
	{
		$readyscript = 'jQuery(document).trigger(' . TJavaScript::quoteString('captchaready:' . $this->getClientID()) . ')';
		$cs = $this->Page->ClientScript;
		$id = $this->getClientID();
		$divid = $id . '_1_recaptchadiv';
		$writer->write('<div id="' . htmlspecialchars($divid) . '">');

		if (!$this->Page->IsCallback) {
			$writer->write(TJavaScript::renderScriptBlock(
					'var RecaptchaOptions = ' . TJavaScript::jsonEncode($this->getClientSideOptions()) . ';'
				));

			$html = $this->recaptcha_get_html($this->getPublicKey());
			/*
			reCAPTCHA currently does not support multiple validations per page
			$html = str_replace(
				array(self::ChallengeFieldName,self::ResponseFieldName),
				array($this->getChallengeFieldName(),$this->getResponseFieldName()),
				$html
			);
			*/
			$writer->write($html);

			$cs->registerEndScript('ReCaptcha::EventScript', 'jQuery(document).ready(function() { ' . $readyscript . '; } );');
		} else {
			$options = $this->getClientSideOptions();
			$options['callback'] = new TJavaScriptLiteral('function() { ' . $readyscript . '; ' . $this->getCallbackScript() . '; }');
			$cs->registerScriptFile('ReCaptcha::AjaxScript', self::RECAPTCHA_JS);
			$cs->registerEndScript('ReCaptcha::CreateScript::' . $id, implode(' ', [
					'if (!jQuery(' . TJavaScript::quoteString('#' . $this->getResponseFieldName()) . '))',
					'{',
					'Recaptcha.destroy();',
					'Recaptcha.create(',
						TJavaScript::quoteString($this->getPublicKey()) . ', ',
						TJavaScript::quoteString($divid) . ', ',
						TJavaScript::encode($options),
					');',
					'}',
				]));
		}

		$writer->write('</div>');
	}


	/**
	 * Gets the challenge HTML (javascript and non-javascript version).
	 * This is called from the browser, and the resulting reCAPTCHA HTML widget
	 * is embedded within the HTML form it was called from.
	 * @param string $pubkey A public key for reCAPTCHA
	 * @param string $error The error given by reCAPTCHA (optional, default is null)
	 * @param bool $use_ssl Should the request be made over ssl? (optional, default is false)
	 * @return string - The HTML to be embedded in the user's form.
	 */
	private function recaptcha_get_html($pubkey, $error = null, $use_ssl = false)
	{
		$server = $use_ssl ? self::RECAPTCHA_API_SECURE_SERVER : $server = self::RECAPTCHA_API_SERVER;
		$errorpart = '';
		if ($error) {
			$errorpart = "&amp;error=" . $error;
		}

		return '<script type="text/javascript" src="' . $server . '/challenge?k=' . $pubkey . $errorpart . '"></script>
		<noscript>
		<iframe src="' . $server . '/noscript?k=' . $pubkey . $errorpart . '" height="300" width="500" frameborder="0"></iframe><br/>
		<textarea name="recaptcha_challenge_field" rows="3" cols="40"></textarea>
		<input type="hidden" name="recaptcha_response_field" value="manual_challenge"/>
		</noscript>';
	}

	/**
	 * Encodes the given data into a query string format
	 * @param $data $data - array of string elements to be encoded
	 * @return string - encoded request
	 */
	private function recaptcha_qsencode($data)
	{
		$req = "";
		foreach ($data as $key => $value) {
			$req .= $key . '=' . urlencode(stripslashes($value)) . '&';
		}

		// Cut the last '&'
		$req = substr($req, 0, strlen($req) - 1);
		return $req;
	}

	/**
	 * Submits an HTTP POST to a reCAPTCHA server
	 * @param string $host
	 * @param string $path
	 * @param array $data
	 * @param int $port port
	 * @return array response
	 */
	private function recaptcha_http_post($host, $path, $data, $port = 80)
	{
		$req = $this->recaptcha_qsencode($data);

		$http_request = "POST $path HTTP/1.0\r\n";
		$http_request .= "Host: $host\r\n";
		$http_request .= "Content-Type: application/x-www-form-urlencoded;\r\n";
		$http_request .= "Content-Length: " . strlen($req) . "\r\n";
		$http_request .= "User-Agent: reCAPTCHA/PHP\r\n";
		$http_request .= "\r\n";
		$http_request .= $req;

		$response = '';
		if (false == ($fs = @fsockopen($host, $port, $errno, $errstr, 10))) {
			die('Could not open socket');
		}

		fwrite($fs, $http_request);

		while (!feof($fs)) {
			$response .= fgets($fs, 1160);
		} // One TCP-IP packet
		fclose($fs);
		$response = explode("\r\n\r\n", $response, 2);

		return $response;
	}

	/**
	 * Calls an HTTP POST function to verify if the user's guess was correct
	 * @param string $privkey
	 * @param string $remoteip
	 * @param string $challenge
	 * @param string $response
	 * @param array $extra_params an array of extra variables to post to the server
	 * @return bool
	 */
	private function recaptcha_check_answer($privkey, $remoteip, $challenge, $response, $extra_params = [])
	{
		//discard spam submissions
		if ($challenge == null || strlen($challenge) == 0 || $response == null || strlen($response) == 0) {
			return false;
		}

		$response = $this->recaptcha_http_post(
			self::RECAPTCHA_VERIFY_SERVER,
			"/recaptcha/api/verify",
		[
			'privatekey' => $privkey,
			'remoteip' => $remoteip,
			'challenge' => $challenge,
			'response' => $response
			] + $extra_params
		);

		$answers = explode("\n", $response [1]);

		if (trim($answers [0]) == 'true') {
			return true;
		} else {
			return false;
		}
	}
}