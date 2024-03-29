<?php
/**
 * @file
 * Importación de empleados desde API externa a estructura Drupal
 *
 * @author Alberto Chaves (mackpipe@gmail.com, https://github.com/mackpipe).
 */

/**
 * @file
 * Import employes rest Api
 */
 

/**
 * import employes - sets up the batch
 */
function employes_rest_api_import() {
	$batch = array(
		'operations' => array(),
		'finished' => 'employes_rest_api_batch_finished', // runs after batch is finished
		'title' => t('Processing Import'),
		'init_message' => t('Import is starting.'),
		'progress_message' => t('Processed @current out of @total.'),
		'error_message' => t('Import has encountered an error.'),
		'file' => drupal_get_path('module', 'employes_rest_api') . '/employes_rest_api_include.inc',
	);

	$url=variable_get('employes_rest_api_url');
	if(!empty($url)&&trim($url)!=""){
		
		$response = file_get_contents($url);
		$employes = json_decode($response);
		
		$progress = 0; // where to start
		$limit = 1; // how many to process for each run
		$max = count($employes); // how many records to process until stop - can do query here to get max times to run
		
		foreach ($employes as $key => $employe) {
				$batch['operations'][] = array('employes_rest_api_import_process', array($progress, $limit,$employe));
				$progress = $progress + $limit;
		}	
	}else{
		drupal_set_message(t('The rest API url was not found, please go to admin / config / system / employes_rest_api and save the configuration'),'error');
	}

	batch_set($batch);
	batch_process('admin/content'); // page to return to after complete 
}

/**
 * this is what runs multiple times per batch
 * progress and limit and is updated during each run
 */
function employes_rest_api_import_process($progress, $limit, $employe, &$context) {
  
	$nid=__employes_rest_api_find_employe($employe->id);
	if($nid==0){
		__employes_rest_api_create_node($employe);
			// update progress for message
		  $progress = $progress + $limit; 
		  // update message during each run so you know where you are in the process
		  $context['message'] = 'Now processing employe [' . $employe->id . ' - '.$employe->employee_name.'] Created';
	}else{
		__employes_rest_api_update_node($employe,$nid);
		// update progress for message
		  $progress = $progress + $limit; 
		  // update message during each run so you know where you are in the process
		  $context['message'] = 'Now processing employe [' . $employe->id . ' - '.$employe->employee_name.'] Updated';
	}

	  		
  
}

/**
 * Finished import employes
 */
function employes_rest_api_batch_finished($success, $results, $operations) {
  if ($success) {
    drupal_set_message('Import is complete');
  }
  else {
    $error_operation = reset($operations);
    $message = t('An error occurred while processing %error_operation with arguments: @arguments', array(
      '%error_operation' => $error_operation[0],
      '@arguments' => print_r($error_operation[1], TRUE)
    ));
    drupal_set_message($message, 'error');
  }
}

/**
 * find employes by id remote
 */
function __employes_rest_api_find_employe($remote_id){

	$query = db_select( 'node', 'n' );
	$query->leftJoin('field_data_field_employes_id_remote2', 'er', 'n.nid = er.entity_id');
	$query
	->fields( 'n', array('nid'))	
	->condition( 'n.type', 'empleados' )
	->condition('er.field_employes_id_remote2_value', $remote_id);
	$result = $query->execute();
	$employe_id  = array();
	foreach( $result as $row ) {
		$employe_id[] = $row->nid;
	}
	if(count($employe_id)>0){
		return $employe_id[0];
	}else{
		return 0;
	}	
}

/**
 * Create node with the info of the employe
 */
function __employes_rest_api_create_node($info){
	global $user;

	$node = new stdClass();
	$node->title = "Employe ".$info->id." from API rest";
	$node->type = "empleados";
	// Sets some defaults. Invokes hook_prepare() and hook_node_prepare().
	node_object_prepare($node); 
	
	$node->language = LANGUAGE_NONE; 
	$node->uid = $user->uid; 
	// Status is 1 or 0; published or not.
	$node->status = 1; 
	// Promote is 1 or 0; promoted to front page or not.
	$node->promote = 0; 
	// Comment is 0, 1, 2; 0 = disabled, 1 = read only, or 2 = read/write.
	$node->comment = 1; 

	// Text field
	$node->field_employes_name2[$node->language][]['value'] = $info->employee_name;

	// Text field
	$node->field_employes_age2[$node->language][]['value'] = $info->employee_age;

	// Text field
	$node->field_employes_id_remote2[$node->language][]['value'] = $info->id;

	// Term reference (taxonomy) field
	$node->field_employes_salary_type[$node->language][]['tid'] = __employes_rest_api_get_term_id_salary($info->employee_salary);

	
	$node = node_submit($node); // Prepare node for saving
	node_save($node);

	$t_args = array('%name' =>$info->id);
	watchdog('node', 'Created node for employe %name.', $t_args, WATCHDOG_NOTICE, ''); 
}

/**
 * Update info of the employe by Id Remote
 */
function __employes_rest_api_update_node($info,$nid){
	global $user;
		
	$node = node_load ($nid);
	
	// Text field
	$node->field_employes_name2[$node->language][]['value'] = $info->employee_name;

	// Text field
	$node->field_employes_age2[$node->language][]['value'] = $info->employee_age;

	// Text field
	$node->field_employes_id_remote2[$node->language][]['value'] = $info->id;

	// Term reference (taxonomy) field
	$node->field_employes_salary_type[$node->language][]['tid'] = __employes_rest_api_get_term_id_salary($info->employee_salary);
	
	node_save($node);

	$t_args = array('%name' =>$info->id);
	watchdog('node', 'Updated node for employe %name.', $t_args, WATCHDOG_NOTICE, '');
}

/**
 * get salary type of employe
 */
function __employes_rest_api_get_term_id_salary($salary) {
	$terms_taxonomy=array('Salario bajo','Salario medio','Salario alto');
	$term_name="";
	if($salary>=0&& $salary<=1000){
		$term_name=$terms_taxonomy[0];
	}
	if($salary>1000&& $salary<=4000){
		$term_name=$terms_taxonomy[1];
	}
	if($salary > 4000){
		$term_name=$terms_taxonomy[2];
	}

	$term_id=__employes_rest_api_get_term_from_name(trim($term_name),'tipo_salario');
	return $term_id;
}

/**
 * Get term id for taxonomy term by name
 */
function __employes_rest_api_get_term_from_name($term_name, $vocabulary_name) {
  if ($vocabulary = taxonomy_vocabulary_machine_name_load($vocabulary_name)) {
    $tree = taxonomy_get_tree($vocabulary->vid);
    foreach ($tree as $term) {
      if ($term->name == $term_name) {
        return $term->tid;
      }
    }
  }

  return false;
}

function _employes_rest_api_curl(){

$Data = array(
	     'service' => '1',
	     'forms_id_form' => '1005',
	     'forms_admins_id_admin' => '36',
	     'content_reg' => array("id_form"=>"1005",
	     						"admins_id_admin"=>"36",
	     						"title_form"=>"Prueba (1005)",
	     						"description_form"=>"listado leads",
	     						"fields_form" =>array(array("type"=>"TEXT","fieldId"=>"fieldType1_combo","name"=>"nombre","value"=>"Wilson A","restrict"=>"NameCharactersAndNumbers"),
	     											  array("type"=>"TEXT","fieldId"=>"fieldType2_combo","name"=>"email","value"=>"wilson@gmail.com","restrict"=>"NameCharactersAndNumbers"),
	     											  array("type"=>"TEXT","fieldId"=>"fieldType3_combo","name"=>"tipodoc","value"=>"CC","restrict"=>"NameCharactersAndNumbers"),
	     											  array("type"=>"TEXT","fieldId"=>"fieldType4_combo","name"=>"numdoc","value"=>"12345678","restrict"=>"NameCharactersAndNumbers")
	     											  ),
	     						"date_form"=>"2019-06-25 00:00:00"
	     						),
	     'date_reg' => '2019-06-26'
	);

	$url = 'aqui va la url';
	$ch = curl_init();								
	# -- -----------------------------------------------------------------------------
	curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
	curl_setopt ($ch, CURLOPT_URL, $url );
	curl_setopt($ch, CURLOPT_POST, 1);
	curl_setopt($ch, CURLOPT_POSTFIELDS, $Data);
	curl_setopt($ch, CURLOPT_SSL_VERIFYPEER, FALSE);
	curl_setopt($ch, CURLOPT_SSL_VERIFYHOST, FALSE);
	$html = curl_exec ( $ch );	
	$error = curl_error($ch); echo $error; 
	curl_close($ch);
	echo $html;

}

function serviceapi_submit($form, &$form_state) {
	//dpm($form_state);
    $options = array();
    $serv = 1;
    $formid = 1005; //$form_state['complete form']['details']['nid']['#value']:
    $form_admin=36;
    $titleform=$form['#node']->title;
    $desc='listado leads';

    $nom= array('type'=>$form['#node']->webform['components'][1]['type'],
    			'fieldId'=>$form['#node']->webform['components'][1]['cid'],
    			'name'=>$form['#node']->webform['components'][1]['form_key'],
    			'value'=>$form_state['input']['submitted']['nombre'],
    			'restrict'=>"NameCharactersAndNumbers");
    //dpm($nom);

    $em=array('type'=>$form['#node']->webform['components'][7]['type'],
    			'fieldId'=>$form['#node']->webform['components'][7]['cid'],
    			'name'=>$form['#node']->webform['components'][7]['form_key'],
    			'value'=>$form_state['input']['submitted']['email'],
    			'restrict'=>"NameCharactersAndNumbers");
    $tip=array('type'=>$form['#node']->webform['components'][6]['type'],
    			'fieldId'=>$form['#node']->webform['components'][6]['cid'],
    			'name'=>$form['#node']->webform['components'][6]['form_key'],
    			'value'=>$form_state['input']['submitted']['tipodoc'],
    			'restrict'=>"NameCharactersAndNumbers");

   $idwebform=0;
	  if ($form_state['input']['submitted']['tipodoc']=='cc' ){
	    $idwebform=3;
	  }elseif($form_state['input']['submitted']['tipodoc']=='ce'){
	    $idwebform=4;
	  }elseif($form_state['input']['submitted']['tipodoc']=='ti'){
	    $idwebform=5;
	  }

	$numd=array('type'=>$form['#node']->webform['components'][$idwebform]['type'],
          'fieldId'=>$form['#node']->webform['components'][$idwebform]['cid'],
          'name'=>'numdoc',
          'value'=>$form_state['values']['submitted'][$idwebform],
          'restrict'=>"NameCharactersAndNumbers");


    
    $dateform=date('Y-m-d H:i:s',$form['#node']->created);
    $datereg=date('Y-m-d',$form_state['build_info']['args'][0]->created);

    $content_reg = array('id_form'=>$formid,
    						'admins_id_admin'=>$form_admin,
    						'title_form'=>$titleform,
    						'description_form'=>$desc,
    						'fields_form'=> array($nom, $em, $tip, $numd),
	    					'date_form'=>$dateform
	    						);
	$content_reg = json_encode($content_reg);   

	//dpm($content_reg);
    // Array keys are matching the key that the remote site accepts. URL encoding will be taken care later.
    $options = array(
    	'service'=>$serv,
    	'forms_id_form'=>$formid,
    	'forms_admins_id_admin'=>$form_admin,
    	'content_reg'=>	$content_reg,
	    'date_reg'=>$datereg					
	    	);
     //drupal_set_message   
    //dpm(json_encode($options));

    $url= 'http://hpl.qailumno.com/SIAF/api/php/proxys/regsproxy.php/';

    $parameters = drupal_http_build_query($options);


    $parameters = $parameters;

    $response= drupal_http_request($url, array(
	'method' => 'POST',
	'data' => $parameters,
	'max_redirects' => 0,
    'headers' => array('Content-Type' => 'application/x-www-form-urlencoded; charset=UTF-8')
    )
	);



  $respuesta=json_decode($response->data,true); 

  $message='Se han registrado tus datos correctamente bajo el id "'.$respuesta['insertedId'].'"';

  drupal_set_message(t($message), 'status');


	

    //dpm($parameters);

    //$options['data'] = http_build_query($options['data']);
    //$options['method'] => 'POST';
    //$url = 'https://[url path to external site]'; 

    // Put your additional headers here. Cookie can be set as well. 
    //$headers = array('Content-Type' => 'application/x-www-form-urlencoded');

    //$options['headers'] => $headers;

    // Submits data to the remote server from the drupal server. User will remain in the Drupal form submission chain.
    //$response = drupal_http_request($url, $options);
   
  // RESTED - PAra probar rest desde chrome
	
}
