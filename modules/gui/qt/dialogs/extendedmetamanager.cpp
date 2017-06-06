/*****************************************************************************
 * extendedmetamanager.cpp : Extended Metadata Manager dialogs
 ****************************************************************************
 * Copyright (C) 2017 Asier Santos Valcárcel
 * Authors: Asier Santos Valcárcel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/extendedmetamanager.hpp"
#include "dialogs_provider.hpp"                 /* THEDP creation */
#include "components/interface_widgets.hpp"     /* CoverArtLabelExt */

#include "input_manager.hpp"
#include "dialogs_provider.hpp" /* THEDP creation */
#include <vlc_playlist.h>  /* playlist_t */
#include "dialogs/fingerprintdialog.hpp" /* fingerprinting */
#include "adapters/chromaprint.hpp" /* fingerprinting */

#include <QMessageBox> //for the Help and About popups

// TODO: al these can probably be deleted
// #include "input_manager.hpp"
// #include <vlc_playlist.h>  /* playlist_t */
// #include <vlc_arrays.h>
// #include <QTabWidget>
// #include <QLabel>
// #include <QGroupBox>
// #include <QDialogButtonBox>
// #include <QPushButton>

#define UNUSED(x) (void)(x) //TODO: delete this. Unused variable warning removal

ExtMetaManagerDialog::ExtMetaManagerDialog( intf_thread_t *_p_intf)
               : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )
{
    msg_Dbg( p_intf, "[ExtMetaManagerDialog] Initializing" ); //TODO: delete this

    // Basic UI setup
    ui.setupUi( this ); //setup the UI from de compiled (.h) version of de QT ui (.ui)
    setWindowFlags( Qt::Tool );
    setWindowRole( "vlc-ext-meta-manager" );
    setWindowTitle( qtr( "Extended Metadata Manager" ) );

    //Button bindings
    BUTTONACT( ui.pushButton_cancel, close() );
    BUTTONACT( ui.pushButton_getFromPlaylist, getFromPlaylist() );
    BUTTONACT( ui.pushButton_getFromFolder, getFromFolder() );
    BUTTONACT( ui.pushButton_searchNow, searchNow() );
    BUTTONACT( ui.pushButton_saveAll, saveAll() );
    BUTTONACT( ui.pushButton_restoreAll, restoreAll() );
    BUTTONACT( ui.pushButton_help, help() );
    BUTTONACT( ui.pushButton_about, about() );
    BUTTONACT( ui.pushButton_clearTable, clearTable() );

    //Events
    CONNECT( ui.tableWidget_metadata, cellClicked(int, int), this, updateArtwork(int, int) );

    //Set de table columns' size
    ui.tableWidget_metadata->setColumnWidth(COL_CHECKBOX, 30); //CheckBox
    ui.tableWidget_metadata->setColumnWidth(COL_TITLE, 200); //Title
    ui.tableWidget_metadata->setColumnWidth(COL_ARTIST, 200); //Artist
    ui.tableWidget_metadata->setColumnWidth(COL_ALBUM, 200); //Album
    ui.tableWidget_metadata->setColumnWidth(COL_GENRE, 120); //Genre
    ui.tableWidget_metadata->setColumnWidth(COL_TRACKNUM, 70); //Track #
    ui.tableWidget_metadata->setColumnWidth(COL_PUBLISHER, 120); //Publisher
    ui.tableWidget_metadata->setColumnWidth(COL_COPYRIGHT, 120); //Copyright
    ui.tableWidget_metadata->setColumnWidth(COL_ARTWORK, 70); //Artwork
    ui.tableWidget_metadata->setColumnWidth(COL_PATH, 50); //Path

    /* ART_URL */
    art_cover = new CoverArtLabelExt( this, p_intf );
    art_cover->setScaledContents(true); //allow the label's image to be scaled (to fit all the area)
    ui.gridLayout_artwork->layout()->addWidget(art_cover);

    workingItems = new vlc_array_t();
    vlc_array_init(workingItems); // Initilize the array for the curretnly working items


    QVLCTools::restoreWidgetPosition( p_intf, "ExtMetaManagerDialog", this );
}

ExtMetaManagerDialog::~ExtMetaManagerDialog()
{
    QVLCTools::saveWidgetPosition( p_intf, "ExtMetaManagerDialog", this );
    msg_Dbg( p_intf, "[ExtMetaManagerDialog] Saving position" ); //TODO: delete this
}

void ExtMetaManagerDialog::toggleVisible()
{
    QVLCDialog::toggleVisible();
    if(isVisible())
        activateWindow();

    msg_Dbg( p_intf, "[ExtMetaManagerDialog] Toggle Visible" ); //TODO: delete this
}

void ExtMetaManagerDialog::close()
{
    msg_Dbg( p_intf, "[ExtMetaManagerDialog] Closing" ); //TODO: delete this
    toggleVisible();
}

void ExtMetaManagerDialog::getFromPlaylist()
{
    clearTable();
    vlc_array_clear(workingItems); //Clear the array with the current working items

    playlist_Lock(THEPL); //Lock the playlist so we can work with it

    int size = THEPL->items.i_size; //Get the size of the playlist
    playlist_item_t *playlist_item;
    input_item_t *input_item;

    if( size ==0 ) return; //if no files selected, finish

    msg_Dbg( p_intf, "[ExtMetaManagerDialog] Clearing array" ); //TODO: delete this
    vlc_array_clear(workingItems); //Clear the array with the current working items

    for(int i = 4;  i <= size+3; i++) //the list starts at 4 because the first 3 are not files
    {
        playlist_item = playlist_ItemGetById(THEPL, i); // Get the playlist_item
        input_item = playlist_item->p_input; // This creates segmentation fault if playlist_item doesnt exist, be careful
        addTableEntry(input_item); //add item to the table
    }

    playlist_Unlock(THEPL); //Unlock the playlist

    //Select the first cell and update artwork label
    ui.tableWidget_metadata->setCurrentCell(0,1);
    updateArtwork(0,0);
}

void ExtMetaManagerDialog::getFromFolder()
{
    // Open a file explorer just with audio files
    QStringList uris = THEDP->showSimpleOpen(
        qtr("Open audio files to manage"),
        EXT_FILTER_AUDIO,
        p_intf->p_sys->filepath );

    if( uris.isEmpty() ) return; //if no files selected, finish

    clearTable();
    vlc_array_clear(workingItems); //Clear the array with the current working items

    //we are going to use the pl to preparse the files, so we clear it first
    // clearPlaylist(); //TODO: this creates seg. fault the second time you use it

    foreach( const QString &uri, uris )
    {
        // Get the item from the URI
        input_item_t *p_item = getItemFromURI(uri.toLatin1().constData());
        addTableEntry(p_item); //Add the item to the table
    }

    //Select the first cell and update artwork label
    ui.tableWidget_metadata->setCurrentCell(0,1);
    updateArtwork(0,0);
}



void ExtMetaManagerDialog::clearPlaylist()
{
    playlist_Lock(THEPL); //Lock the playlist so we can work with it

    int size = THEPL->items.i_size; //Get the size of the playlist
    playlist_item_t *playlist_item;

    if( size ==0 ) return; //If no files, finish

    for(int i = 4;  i <= size+3; i++) //The list starts at 4 because the first 3 are not files
    {
        playlist_item = playlist_ItemGetById(THEPL, i); //Get the playlist_item
        playlist_NodeDelete(THEPL, playlist_item, false); //Delete item from teh playlist
    }

    playlist_Unlock(THEPL); //Unlock the playlist
}

void ExtMetaManagerDialog::searchNow()
{
    msg_Dbg( p_intf, "[ExtMetaManagerDialog] searchNow" ); //TODO: delete this
    if (ui.checkBox_acousticid->isChecked())
    {
        // msg_Dbg( p_intf, "[ExtMetaManagerDialog] acousticId checked" ); //TODO: delete this
        fingerprintTable();
    }
    if (ui.checkBox_TODO->isChecked())
    {
        // msg_Dbg( p_intf, "[ExtMetaManagerDialog] TODO checked" ); //TODO: delete this

    }
    if (ui.checkBox_filenameAnalysis->isChecked())
    {
        // msg_Dbg( p_intf, "[ExtMetaManagerDialog] filenameAnalysis checked" ); //TODO: delete this

    }
}

void ExtMetaManagerDialog::saveAll()
{
    input_item_t *p_item; // This is where the current working item will be
    int rows = ui.tableWidget_metadata->rowCount();
    for(int row = 0;  row < rows; row++) //The list starts at 4 because the first 3 are not files
    {
        p_item = getItemFromRow(row);

        // Check if the row's is checkbox checked
        QCheckBox  *checkbox = (QCheckBox*) ui.tableWidget_metadata->cellWidget(row,COL_CHECKBOX);
        bool is_selected = checkbox->isChecked();

        if (is_selected)
        {
            // Extract information form the table
            const char *title_text = ui.tableWidget_metadata->item(row,COL_TITLE)->text().toLatin1();
            const char *artist_text = ui.tableWidget_metadata->item(row,COL_ARTIST)->text().toLatin1();
            const char *album_text = ui.tableWidget_metadata->item(row,COL_ALBUM)->text().toLatin1();
            const char *genre_text = ui.tableWidget_metadata->item(row,COL_GENRE)->text().toLatin1();
            const char *trackNum_text = ui.tableWidget_metadata->item(row,COL_TRACKNUM)->text().toLatin1();
            const char *publisher_text = ui.tableWidget_metadata->item(row,COL_PUBLISHER)->text().toLatin1();
            const char *copyright_text = ui.tableWidget_metadata->item(row,COL_COPYRIGHT)->text().toLatin1();

            printf("%s\n", title_text); //TODO: delete this
            printf("%s\n", artist_text); //TODO: delete this

            // Set the meta on the item
            input_item_SetTitle(p_item, title_text); //TODO: this creates on hell of a mess with the metadata
            input_item_SetArtist(p_item, artist_text);
            input_item_SetAlbum(p_item, album_text);
            input_item_SetGenre(p_item, genre_text);
            input_item_SetTrackNum(p_item, trackNum_text);
            input_item_SetPublisher(p_item, publisher_text);
            input_item_SetCopyright(p_item, copyright_text);

            input_item_WriteMeta( VLC_OBJECT(THEPL), p_item ); // Write the metas
        }
    }
}

void ExtMetaManagerDialog::fingerprintTable()
{
    msg_Dbg( p_intf, "[ExtMetaManagerDialog] fingerprintTable" ); //TODO: delete this
    input_item_t *p_item; // This is where the current working item will be
    int rows = ui.tableWidget_metadata->rowCount();
    for(int row = 0;  row < rows; row++) //The list starts at 4 because the first 3 are not files
    {
        // Get the item from the current row
        p_item = getItemFromRow(row);
        fingerprint(p_item);
        addTableEntry(p_item, row);


    }
}

void ExtMetaManagerDialog::fingerprint(input_item_t *p_item)
{
    FingerprintDialog *dialog = new FingerprintDialog( this, p_intf, p_item );
    //CONNECT( dialog, metaApplied( input_item_t * ), this, fingerprintUpdate( input_item_t * ) );
    dialog->setAttribute( Qt::WA_DeleteOnClose, true );
    dialog->show();
}

void ExtMetaManagerDialog::restoreAll()
{
    msg_Dbg( p_intf, "[ExtMetaManagerDialog] restoreAll" ); //TODO: delete this
}

void ExtMetaManagerDialog::help()
{
    QMessageBox::information(
      this,
      tr("Help - Extended Metadata Manager"),
      tr(help_text) );
}

void ExtMetaManagerDialog::about()
{
    QMessageBox::information(
      this,
      tr("About - Extended Metadata Manager"),
      tr(about_text) );
}

void ExtMetaManagerDialog::clearTable()
{
    ui.tableWidget_metadata->clearContents();
    ui.tableWidget_metadata->setRowCount(0);
}

void ExtMetaManagerDialog::addTableEntry(input_item_t *p_item)
{
    // Add one row to the table
    int row =   ui.tableWidget_metadata->rowCount();
    ui.tableWidget_metadata->insertRow(row);
    addTableEntry(p_item,row);
}

void ExtMetaManagerDialog::addTableEntry(input_item_t *p_item, int row)
{
    vlc_array_insert(workingItems, p_item, row); //Add item array with the current working items

    // input_item_WriteMeta( VLC_OBJECT(THEPL), p_item); //TODO: write/store edited metadata.

    // Get metadata information from item
    char *title_text = input_item_GetTitle(p_item);
    char *artist_text = input_item_GetArtist(p_item);
    char *album_text = input_item_GetAlbum(p_item);
    char *genre_text = input_item_GetGenre(p_item);
    char *trackNum_text = input_item_GetTrackNum(p_item);
    char *publisher_text = input_item_GetPublisher(p_item);
    char *copyright_text = input_item_GetCopyright(p_item);
    char *uri_text = input_item_GetURI(p_item);

    // Create checkbox for the first column
    QCheckBox  *checkbox = new QCheckBox ();
    checkbox->setChecked(1); // Set checked by default

    // Inserte the obtained values in the table
    ui.tableWidget_metadata->setCellWidget(row, COL_CHECKBOX, checkbox );
    ui.tableWidget_metadata->setItem(row, COL_TITLE, new QTableWidgetItem( title_text ));
    ui.tableWidget_metadata->setItem(row, COL_ARTIST, new QTableWidgetItem( artist_text ));
    ui.tableWidget_metadata->setItem(row, COL_ALBUM, new QTableWidgetItem( album_text ));
    ui.tableWidget_metadata->setItem(row, COL_GENRE, new QTableWidgetItem( genre_text ));
    ui.tableWidget_metadata->setItem(row, COL_TRACKNUM, new QTableWidgetItem( trackNum_text ));
    ui.tableWidget_metadata->setItem(row, COL_PUBLISHER, new QTableWidgetItem( publisher_text ));
    ui.tableWidget_metadata->setItem(row, COL_COPYRIGHT, new QTableWidgetItem( copyright_text ));
    ui.tableWidget_metadata->setItem(row, COL_ARTWORK, new QTableWidgetItem( "**Artwork**" )); //TODO: this must be a file chooser
    ui.tableWidget_metadata->setItem(row, COL_PATH, new QTableWidgetItem( uri_text ));
    ui.tableWidget_metadata->item(row, COL_PATH)->setFlags(0); // Make the path not selectable/editable

    // Free some variables
    delete title_text;
    delete artist_text;
    delete album_text;
    delete genre_text;
    delete trackNum_text;
    delete publisher_text;
    delete copyright_text;
    delete uri_text;
}

void ExtMetaManagerDialog::updateArtwork(int row, int column)
{
    UNUSED(column); //TODO: delete this
    //Get the itme form the row, decode it's Artwork and update it in the UI
    art_cover->showArtUpdate(THEMIM->getIM()->decodeArtURL( getItemFromRow(row) ));
}

input_item_t* ExtMetaManagerDialog::getItemFromRow(int row)
{
    input_item_t *p_item = (input_item_t*)vlc_array_item_at_index(workingItems, row);
    return p_item;
}

input_item_t* ExtMetaManagerDialog::getItemFromURI(const char* uri)
{
    input_item_t *p_item = input_item_New( uri, "Test" ); //TODO: give significant name
    //add to the playlist so it is preparsed (metadata is got)
    playlist_AddInput( THEPL, p_item, false, true );

    //Check it it is preparsed (metadata was added)
    while (!input_item_IsPreparsed(p_item)) ;

    return p_item;
}
