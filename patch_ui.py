import xml.etree.ElementTree as ET
import copy

ui_file = 'src/MainWindow.ui'
tree = ET.parse(ui_file)
root = tree.getroot()

# Find tabSettings layout
for widget in root.iter('widget'):
    if widget.get('name') == 'tabSettings':
        layout = widget.find('layout')
        items = list(layout.findall('item'))
        
        # We need to find btnStartAuth, groupBoxBouyomi, groupBoxObs, verticalSpacer
        # Then we create QToolBox and move groupBoxBouyomi and groupBoxObs inside it.
        
        toolbox = ET.Element('widget', {'class': 'QToolBox', 'name': 'toolBoxSettings'})
        prop_idx = ET.SubElement(toolbox, 'property', {'name': 'currentIndex'})
        num = ET.SubElement(prop_idx, 'number')
        num.text = '0'
        
        bouyomi_item = None
        obs_item = None
        
        for item in items:
            w = item.find('widget')
            if w is not None:
                if w.get('name') == 'groupBoxBouyomi':
                    bouyomi_item = item
                elif w.get('name') == 'groupBoxObs':
                    obs_item = item

        if bouyomi_item is not None and obs_item is not None:
            # Create page 1
            page1 = ET.SubElement(toolbox, 'widget', {'class': 'QWidget', 'name': 'pageBouyomi'})
            attr1 = ET.SubElement(page1, 'attribute', {'name': 'title'})
            str1 = ET.SubElement(attr1, 'string')
            str1.text = '棒読みちゃん連携設定'
            lay1 = ET.SubElement(page1, 'layout', {'class': 'QVBoxLayout', 'name': 'verticalLayoutBouyomi'})
            lay1.append(bouyomi_item)
            
            # Create page 2
            page2 = ET.SubElement(toolbox, 'widget', {'class': 'QWidget', 'name': 'pageObs'})
            attr2 = ET.SubElement(page2, 'attribute', {'name': 'title'})
            str2 = ET.SubElement(attr2, 'string')
            str2.text = 'OBS連携設定'
            lay2 = ET.SubElement(page2, 'layout', {'class': 'QVBoxLayout', 'name': 'verticalLayoutObsContainer'})
            
            # For OBS page, we want to add a URL display and copy button
            # Inside groupBoxObs layout
            obs_layout = obs_item.find('widget').find('layout')
            
            # Create URL item
            url_item = ET.Element('item')
            url_layout = ET.SubElement(url_item, 'layout', {'class': 'QHBoxLayout', 'name': 'horizontalLayoutObsUrl'})
            
            lbl_url = ET.SubElement(url_layout, 'item')
            w_lbl = ET.SubElement(lbl_url, 'widget', {'class': 'QLabel', 'name': 'labelObsUrl'})
            p_lbl = ET.SubElement(w_lbl, 'property', {'name': 'text'})
            s_lbl = ET.SubElement(p_lbl, 'string')
            s_lbl.text = 'ブラウザソースURL:'
            
            txt_url = ET.SubElement(url_layout, 'item')
            w_txt = ET.SubElement(txt_url, 'widget', {'class': 'QLineEdit', 'name': 'editObsUrl'})
            p_txt = ET.SubElement(w_txt, 'property', {'name': 'readOnly'})
            b_txt = ET.SubElement(p_txt, 'bool')
            b_txt.text = 'true'
            p_txt2 = ET.SubElement(w_txt, 'property', {'name': 'text'})
            s_txt2 = ET.SubElement(p_txt2, 'string')
            s_txt2.text = 'http://localhost:8080 (あるいは overlay.html を直接読み込み)' # placeholder
            
            btn_url = ET.SubElement(url_layout, 'item')
            w_btn = ET.SubElement(btn_url, 'widget', {'class': 'QPushButton', 'name': 'btnCopyObsUrl'})
            p_btn = ET.SubElement(w_btn, 'property', {'name': 'text'})
            s_btn = ET.SubElement(p_btn, 'string')
            s_btn.text = 'URLコピー'
            
            # Insert URL item before save button
            obs_layout.insert(len(list(obs_layout))-1, url_item)
            
            lay2.append(obs_item)
            
            # Replace the old items in the main layout with the toolbox
            toolbox_item = ET.Element('item')
            toolbox_item.append(toolbox)
            
            layout.remove(bouyomi_item)
            layout.remove(obs_item)
            
            # Insert toolbox after btnStartAuth
            layout.insert(1, toolbox_item)

tree.write(ui_file, encoding='UTF-8', xml_declaration=True)
print("Patch applied to MainWindow.ui")
