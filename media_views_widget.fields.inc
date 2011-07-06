<?php
/**
 * Implement hook_field_widget_info().
 */
function media_views_widget_field_widget_info() {
  return array(
    'media_generic_alt' => array(
      'label' => t('Media file selector Alternative'),
      'field types' => array('media'),
      'settings' => array(
        'progress_indicator' => 'throbber',
        'allowed_types' => array('image'),
        'allowed_schemes' => array('public', 'private'),
        'view' => '',
      ),
      'behaviors' => array(
        'multiple values' => FIELD_BEHAVIOR_DEFAULT,
        'default value' => FIELD_BEHAVIOR_NONE,
      ),
    ),
  );
}

/*
 * implements hook_field_widget_settings_from
 * 
 * stilling mostly from the standard media field widget
 */
function media_views_widget_field_widget_settings_form($field, $instance) {
  $widget = $instance['widget'];
  $settings = $widget['settings'];
  $form = media_field_widget_settings_form($field, $instance);
  //find all view displays that are blocks
  foreach(views_get_all_views() as $view) {
    $name = $view->name;
    foreach($view->display as $display_name => $display) {
      // only show block displays
      //if($display->display_plugin == 'block')  {
        $view_options[$view->human_name]["$name--$display_name"] = "{$view->human_name} - {$display->display_title}";
      //}
    }
  }
  
  // form element for the view, change this set off an ajax call on the settings
  $form['view'] = array(
    '#type' => 'select',
    '#title' => t('View'),
    '#description' => t('Select the View and Display to use.'),
    '#options' => $view_options,
    '#default_value' => $settings['view'],
  );
  return $form;
}

/**
 * Implements hook_field_widget_form().
 */
function media_views_widget_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {
  $widget_settings = $instance['widget']['settings'];

  $permission = user_access('edit media');
  if (!$permission) {
    // @todo: show the default file upload field.
    return;
  }
  //find the current fid form_state beat element which beat items
  $fid = isset($items[$delta]['fid']) ? $items[$delta]['fid'] : 0;
  $fid = isset($element['#value']['fid']) ? $element['#value']['fid'] : $fid;
  $fid = isset($form_state['values'][$element['#field_name']][$langcode][$delta]['fid']) ?
    $form_state['values'][$element['#field_name']][$langcode][$delta]['fid'] : $fid;


  // if we are rebuilding after a trigger
  // get the action that triggered it as well as
  $action = NULL;
  $action_delta = NULL;
  if(isset($form_state['triggering_element']['#array_parents'])) {
    list($action, $other) = explode("-", end($form_state['triggering_element']['#array_parents']), 2);
    $action_delta = prev($form_state['triggering_element']['#array_parents']);
    reset($form_state['triggering_element']['#array_parents']);
  }

  // if this is a delete then set the fid = 0;
  if(($action_delta == $delta) && ($action == 'delete')) {
    $fid = 0;
    $form_state['values'][$element['#field_name']][$langcode][$delta]['fid'] = $fid;
    $form_state['input'][$element['#field_name']][$langcode][$delta]['fid'] = $fid;
  }

  //wrapper our field so we can find and refesh it
  $wrapper_id = "{$element['#field_name']}-{$delta}";
  $element['#prefix'] = "<div class ='mvw-field' id = '$wrapper_id'>";
  $element['#suffix'] = "<div class ='clearfix'></div></div>";

  //field to store value
  $element['fid'] = array(
    '#type' => 'textfield',
    '#default_value' => $fid,
    '#attributes' => array('class' => array('fid')),
    '#ajax' => array(
      'callback' => "media_views_widget_mvw_update_callback",
      'wrapper' => $wrapper_id,
      'event' => 'refresh',
    ),
  );

  $element["select-$wrapper_id"] = array(
    '#type' => 'button',
    '#value' => t('Select Media'),
    '#ajax' => array(
      'callback' => "media_views_widget_mvw_update_callback",
      'wrapper' => $wrapper_id,
    ),
  );
  if ($fid && ($file = file_load($fid))) {
    //display preview
    //$preview = media_get_thumbnail_preview($file);
    $preview = file_view($file, 'media_preview');
    $element['preview'] = $preview;
    $element['preview']['#weight'] = -100;
    $element['preview']['#prefix'] = '<div class ="mvw-preview">';
    $element['preview']['#suffix'] = '</div>';

    // change title of select
    $element["select-$wrapper_id"]['#value'] = t('Change Media');

    // add edit link
    $element["edit-$wrapper_id"] = array(
      '#type' => 'button',
      '#value' => 'Meta Data',
      '#title' => 'Meta Data',
      '#ajax' => array(
        'callback' => "media_views_widget_mvw_update_callback",
        'wrapper' => $wrapper_id,
      ),
    );

    //add remove link
    $element["delete-$wrapper_id"] = array(
      '#type' => 'button',
      '#value' => 'Remove',
      '#ajax' => array(
        'callback' => "media_views_widget_mvw_update_callback",
        'wrapper' => $wrapper_id,
      ),
   );
  }

  // if the edit action is invoked we populated the dialog
  // we populate the dilog with the media edit form
  if(($action_delta === $delta) && ($action == 'edit')) {
    $file->dialog_inner = "media-dialog-inner-$wrapper_id";
    $dialog_form = drupal_get_form('media_edit', $file);
    $element['dialog'] = array('#markup' => drupal_render($dialog_form));
    $element['dialog'] = media_views_widget_dialog_wrap($wrapper_id, $element['dialog'], FALSE);
  }

  // if the select action is chosen we populate the dialog
  // with the select element
  if(($action_delta === $delta) && ($action == 'select')) {
      $dialog_form = media_views_widget_mvw_media_select("media-dialog-inner-$wrapper_id", 'browser_set', $field, $instance, $langcode, $items, $delta, $element, $file);
    $element['dialog'] = array('#markup' => drupal_render($dialog_form));
    $element['dialog'] = media_views_widget_dialog_wrap($wrapper_id, $element['dialog'], FALSE);
  }

  // If we have not create a dialog create an empty one.
  if(!isset($element['dialog'])) {
    $element['dialog'] = array(
      '#markup' => "",
    );
    $element['dialog'] = media_views_widget_dialog_wrap($wrapper_id, $element['dialog'], FALSE);
  }
  return $element;
}

/*
 * helper function to wrap the dialog in the proper divs so that we can replace stuff
 * as well as attach the dialog
 * PARAM wrapper_id : the wrapper_id for the element associated with this dialog
 * PARAM $element : the dialog element that is to be wrapped
 * PARAM inner_only : only add the dialog_inner not the dialog_wrapper (used when refreshing the dilog but not
 * touching the core element
 *
 * RETURN the $element wraped 
 */
function media_views_widget_dialog_wrap($wrapper_id, $element, $inner_only = TRUE) {
  $element['#prefix'] = isset($element['#prefix']) ? $element['#prefix'] : ''; 
  $element['#suffix'] = isset($element['#suffix']) ? $element['#suffix'] : ''; 
  // add the inner div
  $element['#prefix'] = "<div  class = 'media-dialog-inner' id = 'media-dialog-inner-$wrapper_id'>" . $element['#prefix'];
  $element['#suffix'] .= "</div>";

  // if we are doing thw whole wrapp include the media-dialog-wrapper
  if(!$inner_only) {
    $element['#prefix'] = "<div field = '$wrapper_id' class = 'media-dialog-wrapper' id = 'media-dialog-wrapper-$wrapper_id'>" . $element['#prefix'];
    $element['#suffix'] .= "</div>";
  }

  return $element;
}

/*
 * form for selecting media
 *
 */
function media_views_widget_mvw_media_select($dialog_inner, $active_tab, $field, $instance, $langcode, $items, $delta, $element, $file) {
$form['#type'] = "vertical_tabs";

  
  $widget = $instance['widget'];
  $settings = $widget['settings'];
  $view_string = $settings['view'];
  if(preg_match("/--/", $view_string)) {
    list($name, $display) = explode("--", $view_string);
    }
  $views = array(
    array(
      "name" => $name,
      "display" => $display,
      "title" => 'Browse',
    ),
  /*
    array(
      'name' => 'media_views_widget_default',
      'display' => 'default',
      'title' => 'Browse',
    ),
    array(
      'name' => 'media_views_widget_default',
      'display' => 'block_1',
      'title' => 'Search',
    ),
    */
  );
  foreach ($views as $info) {
    $form[$info['title']] = array(
      '#type' => 'fieldset',
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#title' => $info['title'],
      'browse_form' => drupal_get_form('media_widget_mvw_media_browser',$info['name'], $info['display'], $types, $dialog_inner),
    );
  }
  $types = array('image');
  /*
  $form['edit'] = array(
    '#type' => 'fieldset',
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#title' => 'Meta Data',
    'browse_form' => drupal_get_form('media_edit', $file),
  );
  */
  $form['upload'] = array(
    '#type' => 'fieldset',
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#title' => 'Upload',
    'browse_form' => drupal_get_form('media_add_upload', $types, $dialog_inner),
  );

  return $form;  
}

/*
 * form for views based browser
 */
function media_widget_mvw_media_browser($form, $form_state, $view_name, $view_display_name, $types, $dialog_inner) {
  $form['#dialog_inner'] = $dialog_inner;
  $form['browser']['#markup'] = views_embed_view($view_name, $view_display_name);
  $form['fid'] = array(
    '#type' => 'textfield',
    '#ajax' => array(
       'callback' => 'media_views_widget_mvw_update_from_dialog_callback',
       'wrapper' => $dialog_inner,
       'event' => 'update',
     )
  );
return $form;
}


