<?php
/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009 Fabien Brisset <fbrisset@gmail.com>
 *
 * This file is part of libvalhalla.
 *
 * libvalhalla is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libvalhalla is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libvalhalla; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

    /* set the content type to be XML, so that the browser will recognise it as XML */
    header ("content-type: application/xml; charset=utf-8");

    /* variables definition */
    $allocine_search_url = 'http://www.allocine.fr/recherche/?q=';

    /* function of creation of the document */
    function createDocument ()
    {
        $document = new DomDocument ();
        $document->encoding = "utf-8";

        return $document;
    }

    /* function generating the results tag */
    function generateResultsTag ($keywords, $document)
    {
        $resultsTag = $document->CreateElement ('results');

        $forAttr = $document->CreateAttribute ('for');
        $textForAttr = $document->CreateTextNode ($keywords);
        $forAttr->appendChild ($textForAttr);

        $opSearchAttr = $document->CreateAttribute ('xmlns:opensearch');
        $textOpSearchAttr =
        $document->CreateTextNode ('http://a9.com/-/spec/opensearch/1.1/');
        $opSearchAttr->appendChild ($textOpSearchAttr);

        $resultsTag->appendChild ($forAttr);
        $resultsTag->appendChild ($opSearchAttr);

        return $resultsTag;
    }

    /* generate main xml tag */
    function generateOpenSearchQueryTag ($keywords, $document)
    {
        $queryTag = $document->CreateElement ('opensearch:Query');
        $searchKeyAttr = $document->CreateAttribute ('searchTerms');
        $searchKeyAttrText = $document->CreateTextNode ($keywords);
        $searchKeyAttr->appendChild ($searchKeyAttrText);

        $queryTag->appendChild ($searchKeyAttr);

        return $queryTag;
    }

    /* function generating the total results tag */
    function generateOpenSearchTotResultsTag ($totalRes, $document)
    {
        $totalResults = $document->CreateElement ('opensearch:totalResults');
        $totalResultsText = $document->CreateTextNode ($totalRes);
        $totalResults->appendChild ($totalResultsText);

        return $totalResults;
    }

    /* function generating no search results xml */
    function generate_no_search_results ($keywords)
    {
        $searchResultsDocument = createDocument ();
        $resultsTag = generateResultsTag ($keywords, $searchResultsDocument);
        $opensearchQueryTag = generateOpenSearchQueryTag ($keywords, $searchResultsDocument);
        $openSearchTotResultTag = generateOpenSearchTotResultsTag ('0', $searchResultsDocument);

        $moviematchestag = $searchResultsDocument->CreateElement ('moviematches');
        $movietag = $searchResultsDocument->CreateElement ('movie');
        $content = $searchResultsDocument->CreateTextNode (utf8_encode ('Your query didn\'t return any results.'));
        $movietag->appendChild ($content);
        $moviematchestag->appendChild ($movietag);

        $resultsTag->appendChild ($opensearchQueryTag);
        $resultsTag->appendChild ($openSearchTotResultTag);
        $resultsTag->appendChild ($moviematchestag);

        $searchResultsDocument->appendChild ($resultsTag);

        return $searchResultsDocument->saveXML ();
    }

    /* strip the string in parameter in order to keep the string without allocine specific text */
    function strip_allocine ($string)
    {
        return preg_replace ("# \((\d+)\) - AlloCiné#SUmis", "", trim ($string));
    }

    /* generate the movie tag with the information of the movie */
    function generate_movietag ($allocineid, $document)
    {
        $movietag = $document->CreateElement ('movie');

        $url = 'http://www.allocine.fr/film/fichefilm_gen_cfilm='.$allocineid.'.html';
        $urltag = $document->CreateElement ('url', $url);
        $idtag = $document->CreateElement ('id', $allocineid);
        $type = $document->CreateElement ('type', 'movie');

        $ch = curl_init ();
        curl_setopt ($ch, CURLOPT_URL, $url);
        curl_setopt ($ch, CURLOPT_RETURNTRANSFER, 1);
        $html = curl_exec ($ch);
        curl_close ($ch);
        $data = $html;

        /* title attribute */
        if (preg_match_all ('#<title>(.+)</title>#SUmis', $data, $infos, PREG_SET_ORDER) != 0)
        {
            $alternative_title = $document->CreateElement ('alternative_title', htmlspecialchars (strip_allocine ($infos[0][1])));
            $at_attr = $document->CreateAttribute ('lower');
            $textForAtAttr = $document->CreateTextNode (strtolower (htmlspecialchars (strip_allocine ($infos[0][1]))));
            $at_attr->appendChild ($textForAtAttr);
            $alternative_title->appendChild ($at_attr);
        }
        else
            $alternative_title = $document->CreateElement ('alternative_title', 'Title not found');

        /* french title */
        if (preg_match_all ('#Titre original : <span class="purehtml"><em>(.+)</em></span>#SUmis', $data, $infos, PREG_SET_ORDER) != 0)
             $title = $document->CreateElement ('title',htmlspecialchars (strip_tags ($infos[0][1])));
        else
             $title = $document->CreateElement ('title',htmlspecialchars ($alternative_title->nodeValue));

        $t_attr = $document->CreateAttribute ('lower');
        $textForTattr = $document->CreateTextNode (strtolower ($title->nodeValue));
        $t_attr->appendChild ($textForTattr);
        $title->appendChild ($t_attr);

        /* release date (to be modified) */
        $date = '';
        if (preg_match_all ('#Date de sortie cinéma :(.+)<br />#SUmis', $data, $infos, PREG_SET_ORDER) != 0)
        {
            $date = convert_date (trim (strip_tags ($infos[0][1])));
            $release = $document->CreateElement ('release', $date);
        }
        else
            $release = $document->CreateElement ('release','');

        /* check for people */
        $people = $document->CreateElement ('people', '');

        /* director of movie */
        if (preg_match_all ('#Réalisé par <span class="bold"><a href=\'/personne/fichepersonne_gen_cpersonne=\d+\.html\' title=\'.+\'>.+</span>#SUmis', $data, $infos, PREG_SET_ORDER) != 0)
        {
            if (preg_match_all ('#<a href=\'/personne/fichepersonne_gen_cpersonne=(\d+)\.html\' title=\'(.+)\'>#SUmis', $infos[0][0], $infos_supp, PREG_SET_ORDER) != 0)
            {
                foreach ($infos_supp as $info)
                {
                    $person = $document->CreateElement ('person');
                    $job = $document->CreateAttribute ('job');
                    $textForAttr = $document->CreateTextNode ('director');
                    $job->appendChild ($textForAttr);

                    $name = $document->createElement ('name', $info[2]);
                    $role = $document->createElement ('role', '');
                    $url = $document->createElement ('url','http://www.allocine.fr/personne/fichepersonne_gen_cpersonne='.$info[1].'.html');

                    $person->appendChild ($job);
                    $person->appendChild ($name);
                    $person->appendChild ($role);
                    $person->appendChild ($url);
                    $people->appendChild ($person);
                }
            }
        }

        /* actors information */
        if (preg_match_all ('#Avec.+<a class="underline" href="/film/casting_gen_cfilm=\d+.html">plus</a>#SUmis', $data, $infos, PREG_SET_ORDER) != 0)
        {
            if (preg_match_all ('#<a href=\'/personne/fichepersonne_gen_cpersonne=(\d+)\.html\' title=\'(.+)\'>#SUmis', $infos[0][0], $infos_supp, PREG_SET_ORDER) != 0)
            {
                foreach ($infos_supp as $info)
                {
                    $person = $document->CreateElement ('person');
                    $job = $document->CreateAttribute ('job');
                    $textForAttr = $document->CreateTextNode ('actor');
                    $job->appendChild ($textForAttr);

                    $name = $document->createElement ('name', $info[2]);
                    $role = $document->createElement ('role', '');
                    $url = $document->createElement ('url', 'http://www.allocine.fr/personne/fichepersonne_gen_cpersonne='.$info[1].'.html');

                    $person->appendChild ($job);
                    $person->appendChild ($name);
                    $person->appendChild ($role);
                    $person->appendChild ($url);
                    $people->appendChild ($person);
                }
            }
        }

        /* categories information */
        $categories = $document->CreateElement ('categories', '');
        if (preg_match_all ('#<a href=\'/film/tous/genre-(.+)/\' >(.+)</a>#SUmis', $data, $infos, PREG_SET_ORDER) != 0)
        {
            foreach ($infos as $info)
            {
                $category = $document->CreateElement ('category');
                $category_url = $document->CreateElement ('url', 'http://www.allocine.fr/film/tous/genre-'.$info[1].'/');
                $category_name = $document->CreateElement ('name', $info[2]);
                $category->appendChild ($category_name);
                $category->appendChild ($category_url);
                $categories->appendChild ($category);
            }
        }

        /* homepage information */
        /*
        if (preg_match_all ('#<td valign="top" style="padding:10 0 0 0"><img src=".+" border="0" style="margin: 0 6 0 0" width="4" height="9" class="flechejaune"/><a href="(.+)" class="link1" target="\_blank"><h4><b>Site officiel.+#SUmis', $data, $infos, PREG_SET_ORDER) != 0)
            $homepage = $document->CreateElement ('homepage', strip_tags ($infos[0][1]));
        else
            $homepage = $document->CreateElement ('homepage', '');
        */

        /* description information */
        if (preg_match_all ('#<p><span class="bold">Synopsis : </span>(.+)</p>#SUmis', $data, $infos, PREG_SET_ORDER) != 0)
            $short_overview = $document->CreateElement ('short_overview', strip_tags ($infos[0][1]));
        else
            $short_overview = $document->CreateElement ('short_overview', 'No overview found for this movie');

        /* runtime information */
        if (preg_match_all ('#Durée :.*(\d+)h(\d+) min#SUmis', $data, $infos, PREG_SET_ORDER) != 0)
            $runtime = $document->CreateElement ('runtime',strip_tags ($infos[0][1] * 60 + $infos[0][2]));
        else
            $runtime = $document->CreateElement ('runtime', '');

        /* budget information */
        if (preg_match_all ('#<b>Budget</b> : (.+) \$</h4>#SUmis', $data, $infos,PREG_SET_ORDER) != 0)
            $budget = $document->CreateElement ('budget', preg_replace ("#\s#SUmis", "", strip_tags ($infos[0][1])));
        else
            $budget = $document->CreateElement ('budget', '');

        /* revenue information */
        $revenue = $document->CreateElement ('revenue', '');

        /* revenue fr information */
        /*
        if (preg_match_all ('#<h4><b>Box Office France</b> : (.+) entrées</h4>#SUmis', $data, $infos, PREG_SET_ORDER) != 0)
            $revenuefr = $document->CreateElement ('fr',preg_replace ("#\s#SUmis", "", strip_tags ($infos[0][1])));
        else
            $revenuefr = $document->CreateElement ('fr', '');
        */

        /* revenue usa information */
        /*
        if (preg_match_all ('#<h4><b>Box Office USA</b> : (.+) \$</h4>#SUmis', $data, $infos, PREG_SET_ORDER) != 0)
            $revenueus = $document->CreateElement ('usa',preg_replace ("#\s#SUmis", "", strip_tags ($infos[0][1])));
        else
            $revenueus = $document->CreateElement ('usa', '');

        $revenue->appendChild ($revenuefr);
        $revenue->appendChild ($revenueus);
        */

        /* score information */
        if (preg_match_all ('#Presse.+<span class="moreinfo">\((\d,\d)\)</span>#SUmis', $data, $infos, PREG_SET_ORDER) != 0)
            $score = $document->CreateElement ('score', str_replace (",", ".", strip_tags ($infos[0][1])));
        else
            $score = $document->CreateElement ('score', '');

        /* popularity information */
        if (preg_match_all ('#Spectateurs.+<span class="moreinfo">\((\d,\d)\)</span>#SUmis', $data, $infos, PREG_SET_ORDER) != 0)
            $popularity = $document->CreateElement ('popularity', str_replace (",", ".", strip_tags ($infos[0][1])));
        else
            $popularity = $document->CreateElement ('popularity', '');

        $movietag->appendChild ($score);
        $movietag->appendChild ($popularity);
        $movietag->appendChild ($title);
        $movietag->appendChild ($alternative_title);
        $movietag->appendChild ($type);
        $movietag->appendChild ($idtag);
        $movietag->appendChild ($urltag);
        $movietag->appendChild ($short_overview);
        $movietag->appendChild ($release);
        $movietag->appendChild ($runtime);
        $movietag->appendChild ($budget);
        //$movietag->appendChild ($revenue);
        //$movietag->appendChild ($homepage);
        $movietag->appendChild ($people);
        $movietag->appendChild ($categories);

        $document->appendChild ($movietag);

        /* if the release date is earlier or does not exist, save the file */
        if ($date && (strtotime ($date) - strtotime (date ("Y-m-d")) < 0))
        {
            if (!file_exists ("/tmp/allocine_api"))
                mkdir ("/tmp/allocine_api",0755);
            $fd = fopen ('/tmp/allocine_api/'.$allocineid.".xml", "w");
            if ($fd)
            {
                fwrite ($fd, $document->saveXML ());
                fclose ($fd);
            }
        }

        return $movietag;
    }

    function convert_date ($date)
    {
        $matches = array ();
        $monthes = array ('01' => 'janvier', '02' => 'février', '03' => 'mars', '04' => 'avril', '05' => 'mai', '06' => 'juin', '07' => 'juillet', '08' => 'août', '09' => 'septembre', '10' => 'octobre', '11' => 'novembre', '12' => 'décembre');

        if (!$date)
            return '';

        $matches = preg_split ("#\s#SUmis", $date);
        if (!$matches && sizeof ($matches != 3))
           return '';
        return $matches[2] . '-' . array_search ($matches[1], $monthes) . '-' . str_pad ($matches[0], 2, "0", STR_PAD_LEFT);
    }

    /* function generating the main movie tag */
    function generate_movies_tags ($html_content, $document)
    {
        $moviematchestag = $document->CreateElement ('moviematches');
        $matches = array ();
        preg_match_all ("#<a href='/film/fichefilm_gen_cfilm=(\d+).html'><img.+src='.+'.+alt='.+' />#SUmis", $html_content, $matches);

        foreach ($matches[1] as $resultat)
        {
            if (file_exists ("/tmp/allocine_api/".$resultat.".xml"))
            {
                $doc = createDocument ();
                $doc->load ("/tmp/allocine_api/".$resultat.".xml");
                $nodeList = $doc->getElementsByTagName ('movie')->item (0)->childNodes;
                $movietag = $document->CreateElement ('movie');
                if ($nodeList)
                {
                    foreach ($nodeList as $node)
                    {
                        $tmp = $document->importNode ($node, true);
                        $movietag->appendChild ($tmp);
                    }
                }
            }
            else
                $movietag = generate_movietag ($resultat, $document);

            $moviematchestag->appendChild ($movietag);
        }

        return $moviematchestag;
    }

    /* function generating the searche results tag */
    function generate_search_results ($html_content, $keywords, $number_of_results)
    {
        $searchResultsDocument = createDocument ();
        $resultsTag = generateResultsTag ($keywords, $searchResultsDocument);
        $opensearchQueryTag = generateOpenSearchQueryTag ($keywords, $searchResultsDocument);
        $openSearchTotResultTag = generateOpenSearchTotResultsTag ($number_of_results,$searchResultsDocument);

        $moviematchestag = generate_movies_tags ($html_content, $searchResultsDocument);

        $resultsTag->appendChild ($opensearchQueryTag);
        $resultsTag->appendChild ($openSearchTotResultTag);
        $resultsTag->appendChild ($moviematchestag);

        $searchResultsDocument->appendChild ($resultsTag);

        return $searchResultsDocument->saveXML ();
    }

    /* generate an error xml */
    function error_xml ()
    {
        $errorDocument = createDocument ();

        $error = $errorDocument->CreateElement ('error');
        $content = $errorDocument->CreateTextNode (utf8_encode ('You need to search for something first.'));
        $error->appendChild ($content);
        $errorDocument->appendChild ($error);

        return $errorDocument->saveXML ();
    }

    /* function treating the search results */
    function request_treatment ($html_content, $keywords)
    {
        $matches = array ();
        if (preg_match ("#([0-9]+) résultat[s]{0,1} trouvé[s]{0,1} dans les titres de films.#", $html_content, $matches) == 0)
            return generate_no_search_results ($keywords);
        else
            return generate_search_results ($html_content, $keywords, $matches[1]);
    }

    $matches = array ();
    if (preg_match ("^title=(.+)$^", $_SERVER['QUERY_STRING'], $matches) == 0)
        echo error_xml ();
    else
    {
        $keywords = $matches[1];
        $url = $allocine_search_url.$keywords;
        $ch = curl_init ();

        curl_setopt ($ch, CURLOPT_URL, $url);
        curl_setopt ($ch, CURLOPT_RETURNTRANSFER, 1);
        $html = curl_exec ($ch);
        curl_close ($ch);
        echo request_treatment ($html, urldecode ($keywords));
    }
?>
